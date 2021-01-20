#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "droparea.h"

/* [ PRÓXIMO PASSO ]  - [STATUS]
 * pegar os arquivos arrastados para a janela e enviar direto para o ESP se estiver conectado - WAITING
*/

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->serialPort = new QSerialPort;

    //! Monta o combobox com a lista de portas encontradas
    QStringList ports;

    const auto serialPortInfos = QSerialPortInfo::availablePorts();
    uint8_t i                  = 0;
    for (const QSerialPortInfo &serialPortInfo : serialPortInfos){
        ports.insert(i,serialPortInfo.portName());
        i++;
    }
    ui->comboBox_port->insertItems(0,ports);

    //! Lista de baud rates
    QStringList bauds;
    bauds << "9600" << "19200" << "38400" << "57600" << "115200";
    ui->comboBox_baud->insertItems(0,bauds);


    //!signal e slots
    //! comunicação serial
    connect(ui->pushButton_connect,SIGNAL(clicked(bool)),this,SLOT(connectToSerial()));

    //! lista os arquivos no ESP, se existir algum
    //connect(this->serialPort,SIGNAL(readyRead()),this,SLOT(listFiles()));

    //! envia o comando pro ESP
    //connect(ui->pushButton_exec,SIGNAL(clicked(bool)),this,SLOT(sender())); //envia a mensagem conforme a seleção na ui

    //DEBUG
    abstractLabel = new QLabel(tr("Drag and drop files to field below and WAIT auto-reconnection."));
    abstractLabel->setWordWrap(true);
    abstractLabel->adjustSize();

    /*! Area para soltar o arquivo
        Conecta o sinal do drop area com o método updateFormatsTable
        O sinal envia os dados MIME do arquivo.
        Os connects abaixo estão no novo formato do Qt.
    */
    dropArea = new DropArea;
    connect(dropArea, &DropArea::changed,this, &MainWindow::updateFormatsTable);
    connect(this, &MainWindow::fromSerial,this,&MainWindow::updateFormatsTable);
    QStringList labels;
    labels << tr("Filename") << tr("Path");

    formatsTable = new QTableWidget;
    formatsTable->setColumnCount(2);
    //formatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    formatsTable->setHorizontalHeaderLabels(labels);
    formatsTable->horizontalHeader()->setStretchLastSection(true);

    //clearButton     = new QPushButton(tr("Clear"));
    //copyButton      = new QPushButton(tr("Copy"));
    //quitButton      = new QPushButton(tr("Exit"));
    readButton      = new QPushButton(tr("Read"));
    //writeButton     = new QPushButton(tr("Save"));
    deleteButton    = new QPushButton(tr("Delete"));
    UsageHelpButton = new QPushButton(tr("Help"));

    buttonBox = new QDialogButtonBox;
    //buttonBox->addButton(clearButton, QDialogButtonBox::ActionRole);
    //buttonBox->addButton(copyButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(readButton, QDialogButtonBox::ActionRole);
    //buttonBox->addButton(writeButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(deleteButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(UsageHelpButton, QDialogButtonBox::ActionRole);


#if !QT_CONFIG(clipboard)
    //copyButton->setVisible(false);
#endif

    //buttonBox->addButton(quitButton, QDialogButtonBox::RejectRole);

    //connect(quitButton, &QAbstractButton::clicked, this, &QWidget::close);
    //connect(clearButton, &QAbstractButton::clicked, dropArea, &DropArea::clear);
    //connect(copyButton, &QAbstractButton::clicked, this, &MainWindow::copy);

    connect(this->readButton, SIGNAL(clicked(bool)),   this, SLOT(readFile()));
    connect(this, SIGNAL(filesToWrite(QStringList)),  this, SLOT(writeFile(QStringList)));
    connect(this->deleteButton, SIGNAL(clicked(bool)), this, SLOT(deleteFile()));
    connect(this->UsageHelpButton,SIGNAL(clicked(bool)),this,SLOT(helpButtonSlot()));

    //!Desabilita os botões de comunicação até que esteja conectado
    this->readButton->setDisabled(true);
    //this->writeButton->setDisabled(true);
    this->deleteButton->setDisabled(true);

    /*! Os métodos de interação serial formata a mensagem e emitem um sinal para a escrita.
       O método serialWrite é o único que escreve para a serial, ele gerencia apenas isso e
       alimenta a variável de retorno this->dataFromSerial.
    */
    connect(this,SIGNAL(sendMsg()),this,SLOT(serialWrite()));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(ui->groupBox_connect);

    mainLayout->addWidget(abstractLabel);
    mainLayout->addWidget(dropArea);
    mainLayout->addWidget(formatsTable);
    mainLayout->addWidget(buttonBox);

    ui->centralwidget->setLayout(mainLayout);

    setWindowTitle(tr("ESP File Manager"));
    setMinimumSize(350, 500);

    this->tableSource = "local"; //começa considerando que e local. Se não for, é mudada na conexão.
}

//list files > serialWrite > tableWidget
void MainWindow::listFiles()
{
    this->msg = "^none-l-none-$";
    emit sendMsg();
}

void MainWindow::connectToSerial()
{
    ui->groupBox_connect->setTitle("Connecting...");
    //Se a porta estiver aberta, ele fecha e sai...
    if (this->serialPort->isOpen()){
        this->serialPort->close();
        ui->groupBox_connect->setTitle("Disconnected");
        ui->label_status->setText("Disconnected");
        ui->pushButton_connect->setText("Connect");
        ui->comboBox_baud->setEnabled(true);
        ui->comboBox_port->setEnabled(true);
        this->deleteButton->setDisabled(true);
        this->readButton->setDisabled(true);
        /*
         * Limpa os dados vindos da serial ao desconectar
         *  para não entrar em loop
         */
        this->originalFilename.clear();
        this->renamedFile.clear();
        this->dataFromSerial.clear();
        formatsTable->setRowCount(0);
        return;
    }

    this->deleteButton->setDisabled(false);
    this->readButton->setDisabled(false);
    ui->comboBox_baud->setEnabled(false);
    ui->comboBox_port->setEnabled(false);

    //...se não, pega os valores dos combos para iniciar a conexão...
    this->serialPort->setPortName(ui->comboBox_port->currentText());
    this->serialPort->setBaudRate(ui->comboBox_baud->currentText().toUInt());

    //...e abre a porta aqui.
    if (!this->serialPort->open(QIODevice::ReadWrite)){
        ui->label_status->setText("Failed to connect");
        ui->groupBox_connect->setTitle("Connection failed");
        return;
    }

    //Se conectou, aqui não precisa mais de verificação.
    ui->label_status->setText("Connected");

    //Antes de listar os arquivos, preparar a flag para o table widget saber a origem.
    //const QMimeData *mimeData = new QMimeData();
    listFiles();
    //emit fromSerial(NULL, QString("serial"));
    ui->groupBox_connect->setTitle("Connected");

}

void MainWindow::updateFormatsTable(const QMimeData *mimeData, QString source) //TODO: passar "local" como parametro?
{
    if (source == "local"){
        this->tableLocal(mimeData);
        this->tableSource = "local";
        qDebug() << "LOCAL";
    }
    else if (source == "serial"){
        this->tableSerial();
        this->tableSource = "serial";
        qDebug() << "SERIAL";
    }
}

/*!O sinal emit fromSerial(NULL, QString("serial")) está conectado ao
 * slot updateFormatsTable, que avalia a string pra saber se é "serial" ou "local".
 */
void MainWindow::serialWrite()
{
    qDebug() << "serialWrite started";
    /*! O método serialWrite é invocado pelo sinal sendMsg dos métodos de manipulação
     *  (writeFile, renameFile etc).
     *  Esse método escreve a mensagem, lê a resposta, guarda na stringList @dataFromSerial
     *  e invoca o método @updateFormatsTable para alimentar a tabela através do signal @fromSerial.
    */
    if (!this->serialPort->isOpen()){
        qDebug() << "PORTA FECHADA (serialWrite())"; //TODO: ver quais debugs precisa virar DIALOG
        return;
    }

    ui->pushButton_connect->setText("Disconnect");
    this->serialPort->write(this->msg.toUtf8());
    while (!this->serialPort->waitForBytesWritten(3000));
    while (this->serialPort->waitForReadyRead(2000));
    QByteArray data = this->serialPort->readAll();
    if (this->do_edit){
       this->do_edit = false;
        QMessageBox msgBox;
        msgBox.setText(data.replace("#",""));
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout,[&]{msgBox.close();});
        timer.start(3000);
        msgBox.exec();
        qDebug() << "##########INICIO###########";
        qDebug() << data << " data";
        qDebug() << "###########FIM#############";
    }

    QString clearFirst = QString::fromUtf8(data);
    clearFirst.replace("^","");
    this->dataFromSerial << clearFirst.split("\r\n");
    this->dataFromSerial.removeLast(); //last é vazio, já constatado
    qDebug() << this->dataFromSerial << " dataFromSerial";

    //if (this->dataFromSerial.contains(this->originalFilename)) return;

    emit fromSerial(NULL, QString("serial"));
}

void MainWindow::copy()
{
#if QT_CONFIG(clipboard)
    QString text;
    for (int row = 0, rowCount = formatsTable->rowCount(); row < rowCount; ++row)
        text += formatsTable->item(row, 0)->text() + ": " + formatsTable->item(row, 1)->text() + '\n';
    QGuiApplication::clipboard()->setText(text);
#endif
}

void MainWindow::renameFile(QString orig, QString newName)
{   //REMOVER IF
    //if (this->lastItemChanged.contains(this->renamedFile)){
    //    return;
    //}
    this->lastItemChanged << this->renamedFile;
    this->msg = "^" + orig + "-m-" + newName + "$";
    emit sendMsg();
    QTimer::singleShot(1000, this, &MainWindow::connectToSerial);
    QTimer::singleShot(200, this, &MainWindow::connectToSerial);
}

void MainWindow::deleteFile()
{
    int value = formatsTable->currentRow();
    if (value == -1){
        //TODO: implementar dialogo
        return;
    }
    this->rowColumnNowValues[0] = value;
    this->rowColumnNowValues[1] = 0; //o caminho absoluto para serial está na coluna 0

    qDebug() << this->rowColumnNowValues[0] << " ROW";
    qDebug() << this->rowColumnNowValues[1] << " COL";

    QTableWidgetItem *src = formatsTable->item(this->rowColumnNowValues[0],this->rowColumnNowValues[1]);
    QString target = src->text();
    qDebug() << target << " target";
    qDebug() << "---------";
    this->msg = "^" + target + "-d-none$";
    emit sendMsg();
    QTimer::singleShot(1000, this, &MainWindow::connectToSerial);
    QTimer::singleShot(200, this, &MainWindow::connectToSerial);
}

void MainWindow::readFile()
{
    int value = formatsTable->currentRow();
    if (value == -1){
        //TODO: implementar dialogo
        return;
    }
    this->rowColumnNowValues[0] = value;
    this->rowColumnNowValues[1] = 0; //o caminho absoluto para serial está na coluna 0

    qDebug() << this->rowColumnNowValues[0] << " ROW";
    qDebug() << this->rowColumnNowValues[1] << " COL";

    QTableWidgetItem *src = formatsTable->item(this->rowColumnNowValues[0],this->rowColumnNowValues[1]);
    QString target = src->text();
    qDebug() << target << " target";
    qDebug() << "---------";
    this->msg = "^" + target + "-r-none$";
    this->do_edit = true;
    emit sendMsg();
    QTimer::singleShot(4000, this, &MainWindow::connectToSerial);
    QTimer::singleShot(3000, this, &MainWindow::connectToSerial);
}

void MainWindow::writeFile(QStringList filesToUpload) //perguntar o modo se já existir (append ou overwrite)
{
    /*TODO: se a celula não for selecionada ou se não tiver arquivo, row e column
    retornam -1. Validar isso antes de atribuir à variável rowColumnNow[2]*/

    //0 - garantir que está conectado na serial
    if (!this->serialPort->isOpen()){
        return;
        //TODO: dialog
    }


    //PROCEDIMENTO DE INTERAÇÃO COM A TABLE! IMPORTANTE!
    /*
    int value = formatsTable->currentRow();
    if (value == -1){
        //TODO: implementar dialogo
        return;
    }
    this->rowColumnNowValues[0] = value;
    this->rowColumnNowValues[1] = 1; //o caminho absoluto sempre está na coluna 1
    qDebug() << "write";
    qDebug() << "teste current row";
    qDebug() << this->rowColumnNowValues[0];
    qDebug() << this->rowColumnNowValues[1];
    qDebug() << "---------";
    */

    //1 - ler conteúdo de cada um dos arquivos da lista filesPath
    //2 - enviar em loop o conteúdo, sendo o primeiro com 'w' e o segundo com 'a'
    for (uint8_t i=0;i<filesToUpload.length();i++){
        if (!QFile::exists(filesToUpload.at(i))){
            qDebug() << "arquivo" << filesToUpload << "nao encontrado";
            return;
        }
        QFile file(filesToUpload.at(i));
        file.open(QIODevice::ReadOnly | QIODevice::Text);

        QString flag = "w";
        qDebug() << filesToUpload.at(i);
        while (!file.atEnd()) {
            QByteArray line = file.readLine();
            qDebug() << " <--- LINE";
            this->msg = "^" + filesToUpload.at(i).split("/").last() + "-" + flag + "-" + line.replace("\n","") + "\n$";

            this->serialPort->write(this->msg.toUtf8());
            while (!this->serialPort->waitForBytesWritten(3000));
            while (this->serialPort->waitForReadyRead(2000));
            QByteArray data = this->serialPort->readAll();
            qDebug() << data << " <--- RETORNO";

            flag = "a";
            qDebug() << this->msg;
        }

        file.close();
        qDebug() << "ENVIO DE ARQUIVO CONCLUIDO";
    }
    QTimer::singleShot(600, this, &MainWindow::connectToSerial);
    QTimer::singleShot(1200, this, &MainWindow::connectToSerial);

    //TODO: 3 - pedir para reconectar para ver se os arquivos estão lá
    qDebug() << filesToUpload << " <---- arquivos a enviar";
}

/*! onTableItemChanged
Existem 2 condições para a mudança do valor na célula.

- Se o arquivo foi carregado na aplicação para fazer upload,
então renomeá-la deverá ter efeito local.
- Se o arquivo foi carregado do ESP, então o renome será remoto.
*/
void MainWindow::onTableItemChanged(QTableWidgetItem *item)
{ //voltar pra linha 165 à esquerda

    //!Primeira coisa, pegar o valor que está na célula
    this->renamedFile = item->text();

    /*! onTableItemChanged
    Temos um campo de nome e um campo do caminho absoluto do arquivo.
    Se o renome for feito no campo 'Name', quando pegar o caminho absoluto
    estará com o nome antigo. Esse boolean servirá para informar a origem
    do renome.
    */
    bool nameField = false;
    /*! onTableItemChanged
     *  Utilizado para guardar  o caminho absoluto para replace
     */
    QString fullPath;

    int currrentRow   = formatsTable->currentRow();
    int currentCol    = formatsTable->currentColumn();

    //TRATAMENTO DO PATH SE O  RENAME FOI  FEITO  NA  COLUNA  'NAME'
    /*
     * se o rename foi feito na coluna Name, tem que acertar o nome
     * no caminho absoluto e usar o caminho absoluto  pra  renomear.
     * Se a coluna atual for 0, significa que foi renomeado nela,
     * então pegamos o valor da coluna 1 para fazer o replace e
     * reatribuímos o valor da célula.
     *
     * TODO: acho que resolvi mudança a coluna em outro método.
    */

    //if ()

    /*Renomear a porta serial não é problema. Ao fim, tem que recarregar os
      arquivos com um novo list
    */
    if (currentCol == 0){
        nameField = true;
        QTableWidgetItem *pCell = formatsTable->item(currrentRow,1);
        fullPath                = pCell->text();

        QStringList origFname = this->originalFilename.split("/");

        fullPath.replace(origFname.last(),this->renamedFile);
        pCell->setText(fullPath);
        //se não fosse uma tableWidget já atribuída...
        //formatsTable->setItem(rowColumnNowValues[0],1,pCell);
    }

    //TODO: limpar essa variável quando fizer drop ou descobrir a origem do item de outra maneira
    if (this->dataFromSerial.length() > 0){
        this->renameFile(this->originalFilename,this->renamedFile); //232 à esquerda. TODO: fazer connect
        return;
    }//233 à esquerda
    QString target = nameField ? fullPath : this->originalFilename;
    if (!QFile::exists(this->originalFilename)){
        qDebug() << this->originalFilename << "DEBUG ORIGFNAME";
        qDebug() << "ARQUIVO INEXISTENTE (QFile file)";
        return;
    }
    QFile file(this->originalFilename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.rename(this->renamedFile);
    file.close();

    //... e o resultado...
    qDebug() << this->renamedFile << "renamedFIile...";

}

    //if (this->tableSource == "serial"){}
//} TODO: como descobrir se o dado na lista é serial ou local?

/*void MainWindow::onTableCellClicked(int row, int column)
{//246 à direita
    //this->actualCellContent = formatsTable->item(row, column)->text();
    //qDebug() << this->actualCellContent;
    qDebug() << "ping...";
    //TODO: ou funciona, ou substitui pela posição atual da celula


}*/

void MainWindow::onTableCellDoubleClicked(int row, int column)
{
   //o caminho completo sempre será a coluna 1, por isso já garantimos aqui para renomear
   //se for serial, tem que pegar a coluna 0
   QString tmp =formatsTable->item(row, 1)->text();
   int idx =  tmp == "serial" ? 0 : 1;
   this->originalFilename = formatsTable->item(row, idx)->text();
   qDebug() <<  this->originalFilename << " ORIGINAL FILENAME";
}

void MainWindow::helpButtonSlot()
{
    QMessageBox msgBox;
    msgBox.setText("You can load and rename files without connect to serial, just drag "
                    " and drop it to respective area.\n"
                    "If connected to serial, files on ESP are listed, if exists one.\n"
                    "\n"
                    "Double clique on filename to rename it. Single click to select it. "
                    "Click on delete or read buttons when a filename is selected."
                    "Operation can be slow, but don't worry, this works !\n"
                    "\n"
                    "When finished any operation, serial is automaticly disconnected and"
                    "re-connected, so the files are listed again.\n\n"
                    "Do bit Ao Byte\n"
                    "www.dobitaobyte.com.br\n"
                    "youtube.com/dobitaobytebrasil\n");
    msgBox.exec();
}

void MainWindow::tableSerial()
{
    disconnect(formatsTable, &QTableWidget::itemChanged, this, &MainWindow::onTableItemChanged);
    disconnect(formatsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableCellDoubleClicked);
    //se o texto do botão for "connect" é porque ainda não está conectado. Encerra aqui mesmo.
    if (ui->pushButton_connect->text() == "Connect"){
        return;
    }

    this->originalFilename.clear();
    this->renamedFile.clear();
    formatsTable->setRowCount(0);
    //copyButton->setEnabled(false);

    // informação do mime ---------------------------------------
    if (this->dataFromSerial.length() < 1){
        return;
    }

    //gambi pra não bloquear na função. TODO: colocar dialogo abaixo
    QTimer::singleShot(100, this, &MainWindow::dropHandler);


}

void MainWindow::tableLocal(const QMimeData *mimeData)
{
    disconnect(formatsTable, &QTableWidget::itemChanged, this, &MainWindow::onTableItemChanged);
    disconnect(formatsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableCellDoubleClicked);

    this->originalFilename.clear();
    this->renamedFile.clear();
    formatsTable->setRowCount(0);
    //copyButton->setEnabled(false);
    if (!mimeData){
        qDebug() << "O TIPO MIME NAO VEIO";
        return;
    }

    // informação do mime ---------------------------------------
    QStringList formats = mimeData->formats();

    QStringList toRemove;
    toRemove <<  "application/x-kde4-urilist" << "text/uri-list";

    for  (uint8_t j=0;j<toRemove.length();j++){
        int idx = formats.indexOf(toRemove.at(j));
        if (idx != -1){
            qDebug() << idx;
            formats.removeAll(formats.at(idx));
        }
    }

    uint8_t len = 0;
    filesPath.clear();
    for (const QString &format : formats) {
        QTableWidgetItem *formatItem = new QTableWidgetItem(format);

        formatItem->setFlags(Qt::ItemIsEnabled);
        formatItem->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);

        QString text;
        // informacão do texto (nome, etc) ---------------------------
        if (format == QLatin1String("text/plain")) {
            text = mimeData->text().simplified();
            //remove path do texto
            //int idx = text.lastIndexOf("/");
            //QString filename = text.remove(0,idx+1);
            filesPath << text.replace("file://","").split(" ");


        } else if (format == QLatin1String("text/markdown")) {
            text = QString::fromUtf8(mimeData->data(QLatin1String("text/markdown")));
        } else if (format == QLatin1String("text/html")) {
            qDebug() << "html";
            text = mimeData->html().simplified();
        }
        QStringList textSplit = text.split(" ");
        len = textSplit.length();
        for (uint8_t x=0;x<len;x++){
            QString path     = textSplit.at(x);
            int idx          = path.lastIndexOf("/");

            QString cobaia = path;
            QString filename = cobaia.remove(0,idx+1);

            int row = formatsTable->rowCount();
            formatsTable->insertRow(row);
            formatsTable->setItem(row, 0, new QTableWidgetItem(filename));
            formatsTable->setItem(row, 1, new QTableWidgetItem(path.replace("file://","")));

        }

    }//for
    /*Sinal enviado para @writeFile, encarregado de preparar o conteúdo para envio.
     * O envio é feito exclusivamente pelo método @serialWrite, que é chamado pelo
     * primeiro método após concluir o tratamento.
     * Deverá ocorrer uma série de escritas na serial.
   */
    emit filesToWrite(filesPath);


    connect(formatsTable, &QTableWidget::itemChanged, this, &MainWindow::onTableItemChanged);
    connect(formatsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableCellDoubleClicked);

    formatsTable->resizeColumnToContents(0);
#if QT_CONFIG(clipboard)
    //copyButton->setEnabled(formatsTable->rowCount() > 0);
#endif
}

void MainWindow::dropHandler()
{
    uint8_t len = this->dataFromSerial.length();
    filesPath.clear();
    uint8_t x = 0;
    for (const QString &format : dataFromSerial) {
        QTableWidgetItem *formatItem = new QTableWidgetItem(format);

        formatItem->setFlags(Qt::ItemIsEnabled);
        formatItem->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);

        QString text;
        // informacão do texto (nome, etc) ---------------------------
        if (format.contains(".ini")) {
            text = "parameters";

        }
        else if (format.contains(".txt")) {
            text = "text";
        }
        else if (format.contains(".log")) {
            text = "logs";
        }

        qDebug() << "-------";
        qDebug() << x;
        qDebug() << len;
        QString path     = "serial";

        QString filename = dataFromSerial.at(x);
        qDebug() << filename;
        qDebug() << "-------";

        int row = formatsTable->rowCount();
        formatsTable->insertRow(row);
        formatsTable->setItem(row, 0, new QTableWidgetItem(filename));
        formatsTable->setItem(row, 1, new QTableWidgetItem(path));
        x++;

    }//for
    qDebug() << dataFromSerial;

    connect(formatsTable, &QTableWidget::itemChanged, this, &MainWindow::onTableItemChanged);
    connect(formatsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableCellDoubleClicked);
    //connect(formatsTable, &QTableWidget::cellClicked, this, &MainWindow::onTableCellClicked);
    //esse tb não deu
    //connect(formatsTable,SIGNAL(cellClicked(int,int)),this,SLOT(onTableCellClicked(int,int)));

    formatsTable->resizeColumnToContents(0);

#if QT_CONFIG(clipboard)
    //copyButton->setEnabled(formatsTable->rowCount() > 0);
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "droparea.h"

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
    abstractLabel = new QLabel(tr("Drag and drop files to field below"));
    abstractLabel->setWordWrap(true);
    abstractLabel->adjustSize();

    /*! Area para soltar o arquivo
        Conecta o sinal do drop area com o método updateFormatsTable
        O sinal envia os dados MIME do arquivo
    */
    dropArea = new DropArea;
    connect(dropArea, &DropArea::changed,this, &MainWindow::updateFormatsTable);
    connect(this, &MainWindow::fromSerial,this,&MainWindow::updateFormatsTable);
    QStringList labels;
    labels << tr("Name") << tr("Path");

    formatsTable = new QTableWidget;
    formatsTable->setColumnCount(2);
    //formatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    formatsTable->setHorizontalHeaderLabels(labels);
    formatsTable->horizontalHeader()->setStretchLastSection(true);

    clearButton     = new QPushButton(tr("Clear"));
    copyButton      = new QPushButton(tr("Copy"));
    quitButton      = new QPushButton(tr("Exit"));
    readButton      = new QPushButton(tr("Read"));
    writeButton     = new QPushButton(tr("Save"));
    deleteButton    = new QPushButton(tr("Delete"));
    UsageHelpButton = new QPushButton(tr("Help"));

    buttonBox = new QDialogButtonBox;
    buttonBox->addButton(clearButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(copyButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(readButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(writeButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(deleteButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(UsageHelpButton, QDialogButtonBox::ActionRole);


#if !QT_CONFIG(clipboard)
    copyButton->setVisible(false);
#endif

    buttonBox->addButton(quitButton, QDialogButtonBox::RejectRole);

    connect(quitButton, &QAbstractButton::clicked, this, &QWidget::close);
    connect(clearButton, &QAbstractButton::clicked, dropArea, &DropArea::clear);
    connect(copyButton, &QAbstractButton::clicked, this, &MainWindow::copy);

    connect(this->readButton, SIGNAL(clicked(bool)),   this, SLOT(readFile()));
    connect(this->writeButton, SIGNAL(clicked(bool)),  this, SLOT(writeFile()));
    connect(this->deleteButton, SIGNAL(clicked(bool)), this, SLOT(deleteFile()));
    connect(this->UsageHelpButton,SIGNAL(clicked(bool)),this,SLOT(helpButtonSlot()));

    //!Desabilita os botões de comunicação até que esteja conectado
    this->readButton->setDisabled(true);
    this->writeButton->setDisabled(true);
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
    if (this->serialPort->isOpen()){
        this->serialPort->close();
        ui->label_status->setText("Disconnected");
        ui->pushButton_connect->setText("Connect");
        return;
    }

    this->serialPort->setPortName(ui->comboBox_port->currentText());
    this->serialPort->setBaudRate(ui->comboBox_baud->currentText().toUInt());

    if (!this->serialPort->open(QIODevice::ReadWrite)){
        ui->label_status->setText("Failed to connect");
        return;
    }

    //Se conectou, aqui não precisa mais de verificação.
    ui->label_status->setText("Connected");
    ui->pushButton_connect->setText("Disconnect");

    //Antes de listar os arquivos, preparar a flag para o table widget saber a origem.
    //const QMimeData *mimeData = new QMimeData();
    listFiles();
    //emit fromSerial(NULL, QString("serial"));

}

void MainWindow::sender()
{
    //TODO: implementar todos os tratamentos
    //this->serialPort->write(this->msg.toLocal8Bit());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateFormatsTable(const QMimeData *mimeData, QString source) //TODO: passar "local" como parametro?
{
    if (source == "local"){
        this->tableLocal(mimeData);
        this->tableSource = "local";
        qDebug() << "LOCAL";
    }
    else if (source == "serial"){ //TODO: descomentar AQUI !!!!!!
        //this->tableSerial();
        this->tableSource = "serial";
        qDebug() << "SERIAL";
    }
    else{
        //Não é para haver outra condição, mas em todo o caso, uma segurança a mais.
        return;
    }

}

/*!O sinal emit fromSerial(NULL, QString("serial")) está conectado ao
 * slot updateFormatsTable, que avalia a string pra saber se é "serial" ou "local".
 */
void MainWindow::serialWrite()
{
    //TODO: remover essas duas linhas e colocar timeout pra serial, senao trava a janela
    qDebug() << "escrever";
    return;
    if (!this->serialPort->isOpen()){
        qDebug() << "PORTA FECHADA (serialWrite())";
        return;
    }
    this->serialPort->write(this->msg.toUtf8());
    QByteArray data = this->serialPort->readAll();
    QString clearFirst = QString::fromUtf8(data);
    clearFirst.replace("^","");
    this->dataFromSerial << clearFirst.split("$");

    emit fromSerial(NULL, QString("serial"));
    //TODO: verificar se esse split vai cortar certo ou se vai dar um campo a mais por $ estar no fim

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

void MainWindow::deleteFile()
{
    qDebug() << "delete";
}

void MainWindow::readFile()
{
    qDebug() << "read file";
}

void MainWindow::writeFile() //perguntar o modo se já existir (append ou overwrite)
{
    /*TODO: se a celula não for selecionada ou se não tiver arquivo, row e column
    retornam -1. Validar isso antes de atribuir à variável rowColumnNow[2]*/
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
    */

    //if ()

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
   this->originalFilename = formatsTable->item(row, 1)->text();
   qDebug() <<  this->originalFilename << " ORIGINAL FILENAME";
}

void MainWindow::helpButtonSlot()
{
    QMessageBox msgBox;
    msgBox.setText("Arraste os arquivos para a área designada para carregar na interface.\n"
                    "Duplo click em Nome se deseja renomear o arquivo antes de enviar\n"
                    "\n\n"
                    "Do bit Ao Byte\n"
                    "www.dobitaobyte.com.br\n"
                    "youtube.com/dobitaobytebrasil\n");
    msgBox.exec();
}

void MainWindow::tableSerial()
{
    //se o texto do botão for "connect" é porque ainda não está conectado. Encerra aqui mesmo.
    if (ui->pushButton_connect->text() == "Connect"){
        return;
    }
    this->originalFilename.clear();
    this->renamedFile.clear();
    formatsTable->setRowCount(0);
    copyButton->setEnabled(false);

    // informação do mime ---------------------------------------
    if (this->espFiles.length() < 1){
        return;
    }

    uint8_t len = 0;
    filesPath.clear();
    for (const QString &format : espFiles) {
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
        for (uint8_t x=0;x<len;x++){
            QString path     = ui->comboBox_port->currentText();

            QString filename = espFiles.at(x);

            int row = formatsTable->rowCount();
            formatsTable->insertRow(row);
            formatsTable->setItem(row, 0, new QTableWidgetItem(filename));
            formatsTable->setItem(row, 1, new QTableWidgetItem(path));

        }

    }//for

    connect(formatsTable, &QTableWidget::itemChanged, this, &MainWindow::onTableItemChanged);
    connect(formatsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableCellDoubleClicked);
    //connect(formatsTable, &QTableWidget::cellClicked, this, &MainWindow::onTableCellClicked);
    //esse tb não deu
    //connect(formatsTable,SIGNAL(cellClicked(int,int)),this,SLOT(onTableCellClicked(int,int)));

    formatsTable->resizeColumnToContents(0);

#if QT_CONFIG(clipboard)
    copyButton->setEnabled(formatsTable->rowCount() > 0);
#endif
}

void MainWindow::tableLocal(const QMimeData *mimeData)
{
    this->originalFilename.clear();
    this->renamedFile.clear();
    formatsTable->setRowCount(0);
    copyButton->setEnabled(false);
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

    connect(formatsTable, &QTableWidget::itemChanged, this, &MainWindow::onTableItemChanged);
    connect(formatsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableCellDoubleClicked);

    formatsTable->resizeColumnToContents(0);
#if QT_CONFIG(clipboard)
    copyButton->setEnabled(formatsTable->rowCount() > 0);
#endif
}

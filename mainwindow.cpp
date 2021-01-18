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
    abstractLabel = new QLabel(tr("Arraste para a área abaixo o(s) arquivo(s) a enviar"));
    abstractLabel->setWordWrap(true);
    abstractLabel->adjustSize();

    /* Area para soltar o arquivo
        Conecta o sinal do drop area com o método updateFormatsTable
        O sinal envia os dados MIME do arquivo
    */
    dropArea = new DropArea;
    connect(dropArea, &DropArea::changed,this, &MainWindow::updateFormatsTable);

    QStringList labels;
    labels << tr("Nome") << tr("Caminho");

    formatsTable = new QTableWidget;
    formatsTable->setColumnCount(2);
    //formatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    formatsTable->setHorizontalHeaderLabels(labels);
    formatsTable->horizontalHeader()->setStretchLastSection(true);

    clearButton     = new QPushButton(tr("Limpar"));
    copyButton      = new QPushButton(tr("Salvar"));
    quitButton      = new QPushButton(tr("Sair"));
    readButton      = new QPushButton(tr("Ler"));
    writeButton     = new QPushButton(tr("Escrever"));
    deleteButton    = new QPushButton(tr("Excluir"));
    UsageHelpButton = new QPushButton(tr("Ajuda"));

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

    //formatsTable - signals: itemChanged(QtableWidgetItem *item)
    //connect(this->formatsTable,SIGNAL(cellChanged(int row, int col)),this,SLOT(writeFile()));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(ui->groupBox_connect);

    mainLayout->addWidget(abstractLabel);
    mainLayout->addWidget(dropArea);
    mainLayout->addWidget(formatsTable);
    mainLayout->addWidget(buttonBox);

    ui->centralwidget->setLayout(mainLayout);

    setWindowTitle(tr("ESP File Manager"));
    setMinimumSize(350, 500);
}

void MainWindow::listFiles()
{
    this->msg = "^none-l-none-$";
    emit sendMsg();
}

void MainWindow::connectToSerial()
{
    if (this->serialPort->isOpen()){
        this->serialPort->close();
        ui->label_status->setText("Desconectado");
        ui->pushButton_connect->setText("Conectar");
        return;
    }

    this->serialPort->setPortName(ui->comboBox_port->currentText());
    this->serialPort->setBaudRate(ui->comboBox_baud->currentText().toUInt());

    if (!this->serialPort->open(QIODevice::ReadWrite)){
        ui->label_status->setText("Falha ao tentar conectar");
        return;
    }

    ui->label_status->setText("Conectado");
    ui->pushButton_connect->setText("Desconectar");
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

void MainWindow::updateFormatsTable(const QMimeData *mimeData)
{
    formatsTable->setRowCount(0);
    copyButton->setEnabled(false);
    if (!mimeData)
        return;

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
    qDebug() << "write";
}

void MainWindow::onTableItemChanged(QTableWidgetItem *item)
{
    this->renamedFile = item->text();
    qDebug() << this->renamedFile;


    if (!QFile::exists(this->originalFilename)){
        qDebug() << "ARQUIVO INEXISTENTE (QFile file)";
        return;
    }
    QFile file(this->originalFilename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.rename(this->renamedFile);
    file.close();
}

void MainWindow::onTableCellDoubleClicked(int row, int column)
{
   this->originalFilename = formatsTable->item(row, column)->text();
   qDebug() <<  this->originalFilename << " ORIGINAL FILENAME";
   qDebug() << "filesPath stringlist";
   qDebug() << filesPath.at(0);
   qDebug() << filesPath.at(1);
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

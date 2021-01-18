#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QClipboard>
#include <QMessageBox>
#include <QFile>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QDialogButtonBox;
class QLabel;
class QMimeData;
class QPushButton;
class QTableWidget;
QT_END_NAMESPACE
class DropArea;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QSerialPort *serialPort;

    bool ON = false;

    int rowColumnNowValues[2] = {0};
    QString msg;
    QString originalFilename;
    QString actualCellContent;
    QString renamedFile;
    /* tableSource indica a origem do arquivo para tratar conforme a condição.
     * Se tableSource == 'serial', as ações são na comunicação.
     * Se tableSource == 'local', está carregando arquivos do computador.
    */
    QString tableSource;
    QStringList filesPath;
    QStringList espFiles;
    QStringList dataFromSerial;

    void tableSerial();
    void tableLocal(const QMimeData *mimeData);

public slots:
    void updateFormatsTable(const QMimeData *mimeData, QString source);
    void copy();

    void connectToSerial();
    void deleteFile();
    //void format();
    void listFiles();
    void readFile();
    //void removeAllFiles();
    void sender();
    void writeFile();
    void onTableItemChanged(QTableWidgetItem *item);
    void onTableCellDoubleClicked(int row, int column);
    void onTableCellClicked(int row, int column);
    void helpButtonSlot();
    void serialWrite();

signals:
    void sendMsg();
    void isDone();
    void fromSerial(const QMimeData *mimeData, QString source);
    void signalListFiles();

private:
    Ui::MainWindow *ui;
    DropArea *dropArea;
    QLabel *abstractLabel;
    QTableWidget *formatsTable;

    QPushButton *clearButton;
    QPushButton *copyButton;
    QPushButton *quitButton;
    QPushButton *deleteButton;
    QPushButton *formatButton;
    QPushButton *readButton;
    QPushButton *writeButton;
    QPushButton *UsageHelpButton;

    QDialogButtonBox *buttonBox;
};
#endif // MAINWINDOW_H

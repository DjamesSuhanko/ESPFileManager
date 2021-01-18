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

    QString msg;

public slots:
    void updateFormatsTable(const QMimeData *mimeData);
    void copy();

    void connectToSerial();
    void deleteFile();
    //void format();
    void listFiles();
    void readFile();
    //void removeAllFiles();
    void renameFile();
    void sender();
    void writeFile();
    void onTableItemChanged(QTableWidgetItem *item);

signals:
    void sendMsg();


signals:
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
    QPushButton *renameButton;
    QPushButton *writeButton;

    QDialogButtonBox *buttonBox;
};
#endif // MAINWINDOW_H

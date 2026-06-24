#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort/QtSerialPort>

#include "petprotocol.h"

#include "QTimer"

namespace Ui {
class MainWindow;
}

typedef struct
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} led_color_t;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void output_byte(uint8_t byte);
    void sendfunc(const uint8_t *data, uint16_t len);

    void update_led_color();
private:
    Ui::MainWindow *ui;
    QSerialPort serialPort;
    petcol petcol1;
    QTimer apptimer;

    led_color_t led_color;


//    void extradatacallback1(uint8_t byte);
private slots:
    void handleReadyRead();
    void handleBytesWritten(qint64 bytes);
    void handleError(QSerialPort::SerialPortError error);
    void app_timeout();
    void on_redhorizontalSlider_valueChanged(int value);
    void on_greenhorizontalSlider_valueChanged(int value);
    void on_bluehorizontalSlider_valueChanged(int value);
    void on_sendWindowTitleCheckBox_stateChanged(int arg1);
};

#endif // MAINWINDOW_H

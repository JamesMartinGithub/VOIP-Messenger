#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets/QListWidget>
#include <Controller.h>

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(Controller* pController, QWidget* parent = nullptr);
    ~MainWindow();
    void DisplayInputDevices(std::list<std::wstring> devices);

private slots:
    void DisplayConnectionState(std::string message, Controller::ConnectionType connection, bool isTCP);
    void DisplayMessage(std::string message);
    void on_ipInput_editingFinished();
    void on_nameInput_editingFinished();
    void on_connectButton_clicked();
    void on_serverButton_clicked();
    void on_messageInput_textChanged();
    void on_friendList_itemPressed(QListWidgetItem* item);
    void on_callButton_clicked(bool checked);
    void on_micDropdown_currentIndexChanged(int index);

private:
    bool eventFilter(QObject* object, QEvent* event);

    Controller* controller;
    Ui::MainWindow* ui;
    Controller::ConnectionType currentConnectionState = Controller::ConnectionType::DISCONNECTED;
    std::set<std::string> friends;
    std::string loadedFriend = "";
};

#endif // MAINWINDOW_H

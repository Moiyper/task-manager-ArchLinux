#include "mainwindow.h"
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QTableView>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QHeaderView>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
namespace fs = std::filesystem;

// Структура для хранения данных о процессе
struct ProcData {
    std::string name;
    int pid;
    char state;
    long memory_kb;
};

// Расшифровка состояния процесса
std::string getStateName(char state) {
    switch(state) {
    case 'R': return "Running";
    case 'S': return "Sleeping";
    case 'D': return "Disk Sleep";
    case 'Z': return "Zombie";
    case 'T': return "Stopped";
    case 't': return "Tracing";
    case 'X': return "Dead";
    default:  return "Unknown";
    }
}

// Чтение памяти процесса из /proc/[pid]/status
long getProcessMemory(const std::string& pid_str) {
    std::string status_path = "/proc/" + pid_str + "/status";
    std::ifstream file(status_path);
    std::string line;

    while (std::getline(file, line)) {
        if (line.find("VmRSS:") == 0) {
            // Формат: "VmRSS:     1234 kB"
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string value = line.substr(pos + 1);
                // Убираем пробелы и "kB"
                value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
                value.erase(std::remove(value.begin(), value.end(), 'k'), value.end());
                value.erase(std::remove(value.begin(), value.end(), 'B'), value.end());
                try {
                    return std::stol(value);
                } catch (...) {
                    return 0;
                }
            }
        }
    }
    return 0;
}

// Чтение состояния из /proc/[pid]/stat
char getProcessState(const std::string& pid_str) {
    std::string stat_path = "/proc/" + pid_str + "/stat";
    std::ifstream file(stat_path);
    std::string content;

    if (std::getline(file, content)) {
        // Формат: "pid (comm) state ..."
        size_t first_paren = content.find('(');
        size_t last_paren = content.rfind(')');

        if (first_paren != std::string::npos && last_paren != std::string::npos &&
            last_paren + 2 < content.size()) {
            return content[last_paren + 2]; // Состояние сразу после закрывающей скобки
        }
    }
    return '?';
}

MainWindow::MainWindow(QWidget *parent) {
    std::vector<ProcData> processes;

    // Читаем все процессы
    if (fs::exists("/proc") && fs::is_directory("/proc")) {
        for (const auto& entry : fs::directory_iterator("/proc")) {
            if (entry.is_directory()) {
                std::string dirname = entry.path().filename().string();

                // Проверяем, что это PID (только цифры)
                bool isPID = !dirname.empty() &&
                             std::all_of(dirname.begin(), dirname.end(), ::isdigit);

                if (isPID) {
                    ProcData proc;
                    proc.pid = std::stoi(dirname);

                    // Читаем имя из comm
                    std::ifstream comm_file(entry.path() / "comm");
                    if (comm_file.is_open()) {
                        std::getline(comm_file, proc.name);
                        comm_file.close();
                    } else {
                        proc.name = "unknown";
                    }

                    // Читаем состояние и память
                    proc.state = getProcessState(dirname);
                    proc.memory_kb = getProcessMemory(dirname);

                    processes.push_back(proc);
                }
            }
        }
    }

    // Сортируем по потреблению памяти (от большего к меньшему)
    std::sort(processes.begin(), processes.end(),
              [](const ProcData& a, const ProcData& b) {
                  return a.memory_kb > b.memory_kb;
              });

    setWindowTitle("TaskManager");
    resize(1000, 700);
    setWindowIcon(QIcon(":/icon.png"));

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    QTableView *tableView = new QTableView(this);
    layout->addWidget(tableView);

    // Создаём модель с нужным количеством строк
    QStandardItemModel *model = new QStandardItemModel(processes.size(), 3, this);
    model->setHorizontalHeaderItem(0, new QStandardItem("Process"));
    model->setHorizontalHeaderItem(1, new QStandardItem("Status"));
    model->setHorizontalHeaderItem(2, new QStandardItem("Memory (KB)"));

    // Заполняем таблицу
    for (size_t i = 0; i < processes.size(); i++) {
        const auto& proc = processes[i];

        // Имя процесса
        QStandardItem *nameItem = new QStandardItem(QString::fromStdString(proc.name));
        nameItem->setEditable(false);
        model->setItem(i, 0, nameItem);

        // Статус
        QStandardItem *statusItem = new QStandardItem(QString::fromStdString(getStateName(proc.state)));
        statusItem->setEditable(false);
        model->setItem(i, 1, statusItem);

        // Память
        QStandardItem *memItem = new QStandardItem(QString::number(proc.memory_kb));
        memItem->setEditable(false);
        model->setItem(i, 2, memItem);
    }

    tableView->setModel(model);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->setAlternatingRowColors(true); // Чередование цветов строк
}

MainWindow::~MainWindow() {}
#ifndef	MAINWINDOW_H
#define	MAINWINDOW_H

#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QWidget>

class QAction;
class QLabel;
class QImage;
class QProcess;
class Ui_settingsDialog;

#include "../jungle-board.h"

class Gameboard : public QWidget
{
	Q_OBJECT
public:
	Gameboard();
protected:
	void paintEvent(QPaintEvent*);
	void mousePressEvent(QMouseEvent*);
	void mouseReleaseEvent(QMouseEvent*);
	void mouseMoveEvent(QMouseEvent*);
	QSize	sizeHint() const;
private:
	bool make_move(int,int,int,int);
	int	active_r, active_c;
};

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MainWindow();
	void setSide(colort,colort);
protected:
	void closeEvent(QCloseEvent* event);
	void contextMenuEvent(QContextMenuEvent* event);
private slots:
	void newGame();
	void openGame();
	void saveGame();
	void optionsGame();

	void procReady();
private:
	void createActions();
	void createMenus();
	void createToolBars();
	void createStatusBar();

	QMenu*	fileMenu;
	QMenu*	optionsMenu;
	QMenu*	helpMenu;
	QToolBar*	fileToolBar;
	QAction	*newAct, *openAct, *saveAct, *optionsAct, *quitAct;
	QAction	*aboutAct, *aboutQtAct;
	QLabel	*lstatus, *cstatus, *rstatus;

	Ui_settingsDialog	*settings_ui;
	QDialog	*settingsDialog;

	Gameboard	*gameboard;

	QProcess*	qproc;
};

#endif/*MAINWINDOW_H*/

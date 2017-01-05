// $Id: jungle-ui.cpp,v 1.6 2010/01/16 17:03:50 apollo Exp apollo $
// QT3-based user-interface for Jungle game

// TODO: 
//  - cleanup click-click movement, display of possible moves, etc.
//  - settings --- connect to the rest...
//  - bit-map display of pieces
//  - connect to actual game playing bot!!
//  - save games, open games, ...

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QPushButton>
#include <QStatusBar>

#include <QProcess>

#include "mainwindow.h"
#include "ui_settings_dialogue.h"
#include "../jungle-board.h"
#include "../jungle-search.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static MainWindow*	the_mainwind = 0;

static 	board	the_board;
static	move	available_moves[64];
static	int	num_available_moves;

typedef enum board_state {
	st_initial, st_dragging, st_clicking, st_marked
} board_state;
static board_state	curr_state = st_initial;
static int src_r=-1, src_c=-1, dst_r=-1, dst_c=-1;
static int active_r=-1, active_c=-1;

static bool moveable(int sr, int sc) {
	if (!(sr>=0 && sr<9 && sc>=0 && sc<7))
		return false;
	for (int i=0; i<num_available_moves; ++i)
		if (available_moves[i].from == ((8-sr)*NC+sc))
			return true;
	return false;
}
static bool valid_move(int sr, int sc, int dr, int dc) {
	if (!(sr>=0 && sr<9 && sc>=0 && sc<7
	     && dr>=0 && dr<9 && dc>=0 && dc<7))
		return false;
	for (int i=0; i<num_available_moves; ++i)
		if ((available_moves[i].from == ((8-sr)*NC+sc))
		   && (available_moves[i].to == ((8-dr)*NC+dc)))
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define	RSZ	40
#define	CSZ	40

Gameboard::Gameboard() {
	setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}

QSize Gameboard::sizeHint() const {
	QSize	size(CSZ*7+2,RSZ*9+2);
	return size;
}

static QPen pen_1man() {
	QPen new_pen;
 	//new_pen.setStyle(Qt::DashDotLine);
	new_pen.setWidth(2);
	new_pen.setBrush(Qt::red);
	new_pen.setCapStyle(Qt::RoundCap);
	new_pen.setJoinStyle(Qt::RoundJoin);
	return new_pen;
}
static QPen pen_2man() {
	QPen new_pen;
 	//new_pen.setStyle(Qt::DashDotLine);
	new_pen.setWidth(2);
	new_pen.setBrush(Qt::blue);
	new_pen.setCapStyle(Qt::RoundCap);
	new_pen.setJoinStyle(Qt::RoundJoin);
	return new_pen;
}
static QPen pen_src() {
	QPen new_pen;
 	new_pen.setStyle(Qt::DotLine);
	new_pen.setWidth(4);
	new_pen.setBrush(QColor(100,200,200,127));
	new_pen.setCapStyle(Qt::RoundCap);
	new_pen.setJoinStyle(Qt::RoundJoin);
	return new_pen;
}
static QBrush brush_marked() {
	QBrush new_brush(QColor(255,255,255,32));
	return new_brush;
}
static QPen pen_xmark_good() {
	QPen new_pen;
 	new_pen.setStyle(Qt::DashDotLine);
	new_pen.setWidth(3);
	new_pen.setBrush(QColor(50,250,50,127));
	new_pen.setCapStyle(Qt::RoundCap);
	new_pen.setJoinStyle(Qt::RoundJoin);
	return new_pen;
}
static QPen pen_xmark_bad() {
	QPen new_pen;
 	new_pen.setStyle(Qt::DashDotLine);
	new_pen.setWidth(3);
	new_pen.setBrush(QColor(250,50,50,127));
	new_pen.setCapStyle(Qt::RoundCap);
	new_pen.setJoinStyle(Qt::RoundJoin);
	return new_pen;
}

bool Gameboard::make_move(int sr, int sc, int dr, int dc) {
	if (!(sr>=0 && sr<9 && sc>=0 && sc<7
	     && dr>=0 && dr<9 && dc>=0 && dc<7)) {
		return false;
	}
	for (int i=0; i<num_available_moves; ++i) {
		if ((available_moves[i].from == ((8-sr)*NC+sc))
		   && (available_moves[i].to == ((8-dr)*NC+dc))) {
			struct move m;
			evalt	val;
			m = available_moves[i];
			::make_move(&the_board, m);
			//update((8-(m.from%NC))*CSZ, (m.from/NC)*RSZ, CSZ, RSZ);
			//update((8-(m.to%NC))*CSZ, (m.to/NC)*RSZ, CSZ, RSZ);

			if (the_board.side == black) {
				m = search5(&the_board, &val);
				::make_move(&the_board, m);
				//update((8-(m.from%NC))*CSZ, (m.from/NC)*RSZ, CSZ, RSZ);
				//update((8-(m.to%NC))*CSZ, (m.to/NC)*RSZ, CSZ, RSZ);
			}
			update(0,0,NC*CSZ,NR*RSZ);

			// TODO: cleanup the "update" ...

			the_mainwind->setSide(the_board.side, the_board.xside);
			num_available_moves = gen_moves(&the_board, available_moves);
			return true;
		}
	}
	return false;
}

void Gameboard::paintEvent(QPaintEvent *) {
	/* TODO: double-buffering (?) */
	QPainter	painter(this);

	/* paint it */
	//QColor	river(Qt::blue);
	QColor	river (120,120,220);
	QColor	square( 75, 75, 75);
	QColor	trap  (200,100,200);
	QColor	goal  (250,150,250);
	QColor	colarr[] = {square, river, trap, trap, goal, goal};
	//blank, river, wtrap, btrap, wgoal, bgoal
	painter.drawRect(0,0,7*CSZ,9*RSZ);
	QPen	old_pen = painter.pen();
	for (int r=0; r<9; ++r) {
		for (int c=0; c<7; ++c) {
			painter.drawRect(c*CSZ,r*RSZ,CSZ,RSZ);
			painter.fillRect(c*CSZ+1,r*RSZ+1,CSZ-2,RSZ-2, colarr[squares[(8-r)*NC+c]]);
			if (the_board.colors[(8-r)*NC+c]==white) {
				QFont	old_font = painter.font();
				painter.setFont(QFont("Penguin Attack", 14));
				painter.setPen(pen_1man());
				painter.drawEllipse(c*CSZ+2,r*RSZ+2,CSZ-4,RSZ-4);
				painter.drawText(c*CSZ+CSZ/2, r*RSZ+RSZ/2, QString::number(1+the_board.pieces[(8-r)*NC+c]));
				painter.setPen(old_pen);
				painter.setFont(old_font);
			} else if (the_board.colors[(8-r)*NC+c]==black) {
				QFont	old_font = painter.font();
				painter.setFont(QFont("Penguin Attack", 14));
				painter.setPen(pen_2man());
				painter.drawText(c*CSZ+CSZ/2, r*RSZ+RSZ/2, QString::number(1+the_board.pieces[(8-r)*NC+c]));
				painter.drawEllipse(c*CSZ+2,r*RSZ+2,CSZ-4,RSZ-4);
				painter.setPen(old_pen);
				painter.setFont(old_font);
			}
			if (r==src_r && c==src_c) {
				painter.setPen(pen_src());
				painter.drawEllipse(c*CSZ+5,r*RSZ+5,CSZ-10,RSZ-10);
				painter.setPen(old_pen);
			}
			if (r==dst_r && c==dst_c) {
				painter.fillRect(c*CSZ,r*RSZ,CSZ,RSZ, brush_marked());
			}
			if (curr_state==st_dragging && (r==active_r && c==active_c)) {
				painter.setPen(valid_move(src_r,src_c,r,c) ? pen_xmark_good() : pen_xmark_bad());
				painter.drawLine(c*CSZ,r*RSZ,(c+1)*CSZ,(r+1)*RSZ);
				painter.drawLine((c+1)*CSZ,r*RSZ,c*CSZ,(r+1)*RSZ);
				painter.setPen(old_pen);
			}
		}
	}
}
void Gameboard::mousePressEvent(QMouseEvent* event) {
	int c = event->pos().x() / CSZ;
	int r = event->pos().y() / RSZ;
	switch (curr_state) {
	case st_initial:
		if (!(r>=0 && r<9 && c>=0 && c<7)) break;
		if (!moveable(r,c)) break;
		src_r = r;
		src_c = c;
		curr_state = st_dragging;
		update(src_c*CSZ, src_r*RSZ, CSZ, RSZ);
		break;
	case st_dragging:
		/* shouldn't happen! */
		fprintf(stderr, "MousePressEvent in state st_dragging!?\n");
		break;
	case st_clicking:
		if (!(r>=0 && r<9 && c>=0 && c<7)) break;
		dst_r = r;
		dst_c = c;
		update(dst_c*CSZ, dst_r*RSZ, CSZ, RSZ);
		curr_state = st_marked;
		break;
	case st_marked:
		/* shouldn't happen! */
		fprintf(stderr, "MousePressEvent in state st_marked!?\n");
		break;
	}
}
void Gameboard::mouseReleaseEvent(QMouseEvent* event) {
	int c = event->pos().x() / CSZ;
	int r = event->pos().y() / RSZ;
	switch (curr_state) {
	case st_initial:
		/* do nothing */
		break;
	case st_dragging:
		if (!(r>=0 && r<9 && c>=0 && c<7)) {
			/* released off-board, start over */
			update(src_c*CSZ, src_r*RSZ, CSZ, RSZ);
			src_r = -1;
			src_c = -1;
			curr_state = st_initial;
		} else if (r==src_r && c==src_c) {
			update(src_c*CSZ, src_r*RSZ, CSZ, RSZ);
			curr_state = st_clicking;
		} else {
			dst_r = r;
			dst_c = c;
			make_move(src_r, src_c, dst_r, dst_c);
			update(src_c*CSZ, src_r*RSZ, CSZ, RSZ);
			update(dst_c*CSZ, dst_r*RSZ, CSZ, RSZ);
			src_r = src_c = dst_r = dst_c = -1;
			curr_state = st_initial;
		}
		break;
	case st_clicking:
		/* shouldn't happen! */
		fprintf(stderr, "MouseReleaseEvent in state st_clicking!?\n");
		break;
	case st_marked:
		if (r==dst_r && c==dst_c) {
			make_move(src_r, src_c, dst_r, dst_c);
			update(src_c*CSZ, src_r*RSZ, CSZ, RSZ);
			update(dst_c*CSZ, dst_r*RSZ, CSZ, RSZ);
			src_r = src_c = dst_r = dst_c = -1;
			curr_state = st_initial;
		} else {
			/* start over */
			curr_state = st_clicking;
			update(src_c*CSZ, src_r*RSZ, CSZ, RSZ);
			update(dst_c*CSZ, dst_r*RSZ, CSZ, RSZ);
		}
		break;
	}
}
void Gameboard::mouseMoveEvent(QMouseEvent* event) {
	int	old_c = active_c, old_r = active_r;
	active_c = event->pos().x() / CSZ;
	active_r = event->pos().y() / RSZ;
	if (old_c>=0 && old_r>=0 && old_c<7 && old_r<9)
		update(old_c*CSZ, old_r*RSZ, CSZ, RSZ);
	if (active_c>=0 && active_r>=0 && active_c<7 && active_r<9)
		update(active_c*CSZ, active_r*RSZ, CSZ, RSZ);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow() {
	gameboard = new Gameboard;
	setCentralWidget(gameboard);
	createActions();
	createMenus();
	createToolBars();
	createStatusBar();
	setWindowTitle(tr("Jungle"));

	settingsDialog = 0;
	settings_ui = 0;

	qproc = new QProcess;
	
	connect(qproc, SIGNAL(readyReadStandardOutput()), this, SLOT(procReady()));
	qproc->start("echo");
}
void MainWindow::closeEvent(QCloseEvent* event) {
	event->accept();
	//event->ignore();
}
void MainWindow::contextMenuEvent(QContextMenuEvent* ) {
}
void MainWindow::createActions() {
	newAct = new QAction(tr("&New"), this);
	newAct->setIcon(QIcon(":/images/new.png"));
	newAct->setShortcut(QKeySequence::New);
	newAct->setStatusTip(tr("Start a new game."));
	connect(newAct, SIGNAL(triggered()), this, SLOT(newGame()));

	openAct = new QAction(tr("&Open"), this);
	openAct->setIcon(QIcon(":/images/open.png"));
	openAct->setShortcut(QKeySequence::Open);
	openAct->setStatusTip(tr("Open a saved game."));
	connect(openAct, SIGNAL(triggered()), this, SLOT(openGame()));

	saveAct = new QAction(tr("&Save"), this);
	saveAct->setIcon(QIcon(":/images/save.png"));
	saveAct->setShortcut(QKeySequence::Save);
	saveAct->setStatusTip(tr("Save the current game."));
	connect(saveAct, SIGNAL(triggered()), this, SLOT(saveGame()));

	optionsAct = new QAction(tr("&Options"), this);
	//optionsAct->setIconSet(QPixmap::fromMimeSource("options.png"));
	optionsAct->setShortcut(tr("Ctrl+O"));
	optionsAct->setStatusTip(tr("Options for the game."));
	connect(optionsAct, SIGNAL(triggered()), this, SLOT(optionsGame()));

	quitAct = new QAction(tr("&Quit"), this);
	//quitAct->setIconSet(QPixmap::fromMimeSource("quit.png"));
	quitAct->setShortcut(tr("Ctrl+Q"));
	quitAct->setStatusTip(tr("Quit the program."));
	connect(quitAct, SIGNAL(triggered()), this, SLOT(close()));
}
void MainWindow::createMenus() {
	fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(newAct);
	fileMenu->addAction(openAct);
	fileMenu->addAction(saveAct);
	fileMenu->addSeparator();
	fileMenu->addAction(optionsAct);
	fileMenu->addSeparator();
	fileMenu->addAction(quitAct);
}
void MainWindow::createToolBars() {
}
void MainWindow::createStatusBar() {
	lstatus = new QLabel(tr(" Blue "));
	lstatus->setAlignment(Qt::AlignHCenter);
	lstatus->setMinimumSize(lstatus->sizeHint());
	lstatus->setText(tr(" Red "));

	cstatus = new QLabel("xxxxx");
	cstatus->setIndent(3);

	rstatus = new QLabel(tr(" MOD "));
	rstatus->setAlignment(Qt::AlignHCenter);
	rstatus->setMinimumSize(rstatus->sizeHint());

	statusBar()->addWidget(lstatus);
	statusBar()->addWidget(cstatus, 1);
	statusBar()->addWidget(rstatus);
}
void MainWindow::setSide(colort side, colort xside) {
	if (side==white)
		lstatus->setText(tr(" Red "));
	else if (side==black)
		lstatus->setText(tr(" Blue "));
	else {
		if (xside==white)
			lstatus->setText(tr("<B>RED WINS</B>"));
		else if (xside==black)
			lstatus->setText(tr("<B>BLUE WINS</B>"));
		else
			lstatus->setText(tr("<B>DRAW</B>"));
	}
}
void MainWindow::newGame() {
	init_board(&the_board);
	curr_state = st_initial;
	src_r = src_c = dst_r = dst_c = active_r = active_c = -1;
	gameboard->update(0,0,NC*CSZ,NR*RSZ);
	num_available_moves = gen_moves(&the_board, available_moves);
	setSide(the_board.side,the_board.xside);
}
void MainWindow::openGame() {
	fprintf(stderr, "<openGame>");
	qproc->write("<openGame>");
}
void MainWindow::saveGame() {
	show_game(stderr, &the_board);
}
void MainWindow::optionsGame() {
	if (!settingsDialog) {
		settings_ui = new Ui_settingsDialog();
		settingsDialog = new QDialog;
		settings_ui->setupUi(settingsDialog);
	}
	settingsDialog->show();
	settingsDialog->raise();
	settingsDialog->activateWindow();
}
void MainWindow::procReady() {
	QByteArray ba = qproc->readAll();
	char*	data = ba.data();
	fprintf(stderr, "{%i}\"%s\"\n", strlen(data), data);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	QApplication	app(argc,argv);

	init_hash();
	init_board(&the_board);
	num_available_moves = gen_moves(&the_board, available_moves);

	settings.depth = 700;
	settings.f_tt = 1;
	settings.f_id = 1;
	settings.id_base = 300;
	settings.id_step = 200;
	settings.f_quiesce = 1;
	settings.qdepth = 400;
	settings.f_nmp = 1;
	settings.nmp_R1 = 300;
	settings.nmp_cutoff = 400;
	settings.nmp_R2 = 200;
	settings.evaluator = 1;

	MainWindow	mainwind;
	the_mainwind = &mainwind;
	mainwind.show();

	return app.exec();
}

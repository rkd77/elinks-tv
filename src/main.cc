/* (C) 2014 Witold Filipczyk */

#include <langinfo.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#define Uses_TApplication
#define Uses_TButton
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TEditor
#define Uses_TFrame
#define Uses_TKeys
#define Uses_TKeys_Extended
#define Uses_TMenuBar
#define Uses_TMenuBox
#define Uses_TMenuItem
#define Uses_TMenuSubItem
#define Uses_MsgBox
#define Uses_TScrollBar
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#define Uses_TWindow
#define Uses_TVCodePage
#define Uses_TVIntl
#include <tv.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

#include "setup.h"

#include "util/math.h"
#include "osdep/generic.h"
#include "osdep/types.h"
#include "bfu/leds.h"
#include "bookmarks/dialogs.h"
#include "cache/dialogs.h"
#include "config/cmdline.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "cookies/dialogs.h"
#include "dialogs/document.h"
#include "dialogs/download.h"
#include "dialogs/info.h"
#include "dialogs/menu.h"
#include "dialogs/options.h"
#include "document/document.h"
#include "document/view.h"
#include "formhist/dialogs.h"
#include "globhist/dialogs.h"
#include "protocol/auth/dialogs.h"
#include "session/session.h"
#include "main/interlink.h"
#include "main/main.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "terminal/itrm.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "viewer/action.h"
#include "viewer/text/draw.h"
#include "viewer/text/search.h"
#include "viewer/text/vs.h"
#include "viewer/text/view.h"


static bool linux_console_8859_2 = false;
static bool initNext = false;

static int ac;
static char **av;

static unsigned char remap_char[256];
static unsigned char remap_frame[256];
static unsigned char remap_letter[256];

enum
{
	cmNewWindow = 101,
	cmMyNewTab,
	cmMyNewTabBackground,
	cmGotoUrl,
	cmGoBack,
	cmGoForward,
	cmHistory,
	cmUnHistory,

	cmSaveUrlAs,
	cmSaveFormatted,
	cmKillBackgroundConnections,
	cmFlushAllCaches,
	cmResourceInfo,
	cmOSShell,
	cmResizeTerminal,

	cmSearch,
	cmSearchBackward,
	cmFindNext,
	cmFindPrevious,
	cmTypeaheadSearch,
	cmToggleHtml,
	cmToggleImages,
	cmToggleLinkNumbering,
	cmToggleDocumentColors,
	cmToggleWrap,
	cmDocumentInfo,
	cmReloadDocument,
	cmRerenderDocument,
	cmFrameFullScreen,
	cmNextTab,
	cmPrevTab,
	cmCloseTab,

	cmLinkSelected,

	cmHistory2,
	cmBookmarks,
	cmCache,
	cmDownloads,
	cmCookies,
	cmFormHistory,
	cmAuthentication,

	cmLanguage,
	cmCharacterSet,
	cmTerminalOptions,
	cmFileExtensions,
	cmOptionsManager,
	cmKeybindingManager,
	cmSaveOptions,

	cmHomePage,
	cmDocumentation,
	cmKeys,
	cmLedIndicators,
	cmBugs,
	cmCopying,
	cmAuthors,
	cmAbout,

	cmQuit2,
};

static void init_next(void);

short color_values[] = {
	0,//0
	4,//1
	2,//2
	6,//3 //6
	1,//4
	5,//5
	3,//6
	7//7
};

bool initialized = false;

class TFelinks: public TApplication
{
public:
	TFelinks();
	~TFelinks();
	static TMenuBar* initMenuBar(TRect r);
	static TStatusLine* initStatusLine(TRect r);
	void handleEvent(TEvent &event);
	void aboutDlgBox();
	void createNewWindow();
                                      // Previous callback in the code page chain
	static TVCodePageCallBack oldCPCallBack;
	static void cpCallBack(unsigned short *map); // That's our callback
};

class TTermView;
class TTermScrollBar;

class TFelinksWindow: public TWindow
{
public:
	TFelinksWindow(TRect& bounds, char* str, int num);
	~TFelinksWindow();
	void handleEvent(TEvent &event);
	void setTitle(char *text);
	void dragView(TEvent& event, uchar mode, TRect& limits, TPoint minSize, TPoint maxSize);
	TTermScrollBar *standardScrollBar2(unsigned short aOptions);
	void zoom();
	TTermView *tw;
};

class TTermView: public TView
{
public:
	TTermView(TRect& bounds, TFelinksWindow *p, TTermScrollBar *pion1, TTermScrollBar *poziom1);
	~TTermView();
	void handleEvent(TEvent &event);
	char *getMouseSequence(TEvent &event);
	void zmienRozmiar(TPoint &p);
	void draw();
	void changePion(int ny);
	void changePoziom(int nx);
	TFelinksWindow *pWindow;
	TTermScrollBar *pion, *poziom;
	struct terminal *term;
	struct session *ses;
	struct document_view *doc_view;
	struct itrm *itrm;
};

class TTermScrollBar: public TScrollBar
{
public:
	TTermScrollBar(const TRect& r);
	~TTermScrollBar();
	void handleEvent(TEvent &event);
	TTermView *pWindow;
};

TVCodePageCallBack TFelinks::oldCPCallBack = NULL;

typedef unsigned char para[2];

static unsigned char remap_pairs_8859_1[][2] =
{
	{0, 0},
};

static unsigned char remap_pairs_frames_8859_1[][2] =
{
	{'j', 137},
	{'k', 140},
	{'l', 134},
	{'m', 131},
	{'n', 143},
	{'q', 138},
	{'t', 135},
	{'u', 141},
	{'v', 139},
	{'w', 142},
	{'x', 133},
	{0, 0},
};

static unsigned char remap_pairs_letters_8859_1[][2] =
{
	{0, 0},
};

static unsigned char remap_pairs_8859_2[][2] =
{
	{16, 195},
	{17, 194},
	{18, 193},
	{24, 196},
	{30, 196},
	{31, 193},
	{177, 197},
	{178, 199},
	{179, 223},
	{180, 218},
	{186, 223},
	{187, 222},
	{188, 209},
	{191, 222},
	{192, 208},
	{195, 219},
	{196, 204},
	{200, 208},
	{201, 221},
	{205, 204},
	{217, 209},
	{218, 221},
	{254, '*'},
	{0, 0},
};

static unsigned char remap_pairs_frames_8859_2[][2] =
{
	{'a', 177},
	{'j', 217}, ///209},//217},
	{'k', 191},
	{'l', 218}, ///221},//218},
	{'m', 192},
	{'n', 197},
	{'q', 196},
	{'t', 195},
	{'u', 180}, ///218},//180},
	{'v', 193},
	{'w', 194},
	{'x', 179},///223},//179},
	{0, 0},
};

static unsigned char remap_pairs_letters_8859_2[][2] =
{
	{198, 6},
	{202, 10},
	{209, 17},
	{211, 19},
	{0, 0},
};

static void init_remap_chars(void)
{
	unsigned char remap_frame_tmp[256];
	para *remap_pairs;
	para *remap_pairs_frames;
	para *remap_pairs_letters;

	for (int i = 0; i <= 255; ++i)
	{
		remap_char[i] = (unsigned char)i;
		remap_frame_tmp[i] = (unsigned char)i;
		remap_letter[i] = (unsigned char)i;
		remap_frame[i] = (unsigned char)i;
	}

	if (linux_console_8859_2)
	{
		remap_pairs = remap_pairs_8859_2;
		remap_pairs_frames = remap_pairs_frames_8859_2;
		remap_pairs_letters = remap_pairs_letters_8859_2;
	}
	else
	{
		remap_pairs = remap_pairs_8859_1;
		remap_pairs_frames = remap_pairs_frames_8859_1;
		remap_pairs_letters = remap_pairs_letters_8859_1;
	}

	for (int i = 0; remap_pairs[i][0] != remap_pairs[i][1]; ++i)
	{
		remap_char[remap_pairs[i][0]] = remap_pairs[i][1];
	}
	for (int i = 0; remap_pairs_frames[i][0] != remap_pairs_frames[i][1]; ++i)
	{
		remap_frame_tmp[remap_pairs_frames[i][0]] = remap_pairs_frames[i][1];
	}
	for (int i = 0; remap_pairs_letters[i][0] != remap_pairs_letters[i][1]; ++i)
	{
		remap_letter[remap_pairs_letters[i][0]] = remap_pairs_letters[i][1];
	}

	for (int i = 0; i <= 255; ++i)
	{
		remap_frame[i] = remap_char[remap_frame_tmp[i]];
	}
}

static unsigned char ourRemapChar(unsigned char ch, unsigned short *map)
{
	return remap_char[ch];
}

short tv_frames(short c)
{
	return remap_frame[c];
}

static void ourRemapString(unsigned char *out, unsigned char *in, unsigned short *map)
{
	for (int i = 0; in[i]; ++i)
	{
		out[i] = ourRemapChar(in[i], map);
	}
}

void TFelinks::cpCallBack(unsigned short *map)
{
	TDeskTop::defaultBkgrnd = 199;
	ourRemapString((unsigned char *)TFrame::zoomIcon, (unsigned char *)TFrame::ozoomIcon, map);
	ourRemapString((unsigned char *)TFrame::unZoomIcon, (unsigned char *)TFrame::ounZoomIcon, map);
	ourRemapString((unsigned char *)TFrame::frameChars, (unsigned char *)TFrame::oframeChars, map);
	ourRemapString((unsigned char *)TMenuBox::frameChars, (unsigned char *)TMenuBox::oframeChars, map);
	ourRemapString((unsigned char *)TFrame::dragIcon, (unsigned char *)TFrame::odragIcon, map);
	ourRemapString((unsigned char *)TScrollBar::vChars, (unsigned char *)TScrollBar::ovChars, map);
	ourRemapString((unsigned char *)TScrollBar::hChars, (unsigned char *)TScrollBar::ohChars, map);

	if (oldCPCallBack) oldCPCallBack(map);
}

TFelinks *mainFelinks = NULL;

TFelinks::TFelinks(): TProgInit(&TFelinks::initStatusLine, &TFelinks::initMenuBar,
	&TFelinks::initDeskTop)/*, windowCommands(getCommands()*)*/
{
	mainFelinks = this;
}

TFelinks::~TFelinks()
{
	terminate_all_subsystems();
	program.terminate = 1;
}

ushort executeDialog( TDialog* pD, void* data=0 )
{
	ushort c = cmCancel;

	if (TProgram::application->validView(pD))
	{
		if (data) pD->setData(data);
		c = TProgram::deskTop->execView(pD);
		if ((c != cmCancel) && (data))
			pD->getData(data);
		CLY_destroy(pD);
	}
	return c;
}

void TFelinks::aboutDlgBox()
{
	TDialog *aboutBox = new TDialog(TRect(0, 0, 39, 12), "About");
	aboutBox->insert(
	new TStaticText(TRect(9, 2, 30, 9),
	"\003Felinks www browser\n\n"       // These strings will be
	"\003 \n\n"             // concatenated by the compiler.
	"\003Copyright (c) 2014\n\n"      // The \003 centers the line.
	"\003Witold Filipczyk"
	));

	aboutBox->insert(
	new TButton(TRect(14, 9, 26, 11), " OK", cmOK, bfDefault)
	);
	aboutBox->options |= ofCentered;
	executeDialog(aboutBox);
}

void *currentFWindow;

static const char *getKeySequence(TEvent &event)
{
	const char *buffer = NULL;

	if (event.keyDown.charScan.charCode > 0)
	{
		if (event.keyDown.charScan.charCode == '\n')
		{
			buffer = "\r";
		}
		else if (event.keyDown.charScan.charCode == 127)
		{
			buffer = "\b";
		}
		else
		{
			static char buf[2];

			buf[0] = (char)event.keyDown.charScan.charCode;
			buf[1] = 0;
			buffer = buf;
		}
		return buffer;
	}

	switch (event.keyDown.keyCode)
	{
	case kbEsc:
		buffer = "\e";
		break;
	case kbUp:
		buffer="\e[A";
		break;
	case kbDown:
		buffer="\e[B";
		break;
	case kbRight:
		buffer="\e[C";
		break;
	case kbLeft:
		buffer="\e[D";
		break;
	case kbBackSpace:
		buffer="\b";
		break;
	case kbInsert:
		buffer="\e[2~";
		break;
	case kbDelete:
		buffer="\e[3~";
		break;
	case kbHome:
		buffer="\e[7~";
		break;
	case kbEnd:
		buffer="\e[8~";
		break;
	case kbPgUp:
		buffer="\e[5~";
		break;
	case kbPgDn:
		buffer="\e[6~";
		break;
	case kbF1:
		buffer="\e[11~";
		break;
	case kbF2:
		buffer="\e[12~";
		break;
	case kbF3:
		buffer="\e[13~";
		break;
	case kbF4:
		buffer="\e[14~";
		break;
	case kbF5:
		buffer="\e[15~";
		break;
	case kbF6:
		buffer="\e[17~";
		break;
	case kbF7:
		buffer="\e[18~";
		break;
	case kbF8:
		buffer="\e[19~";
		break;
	case kbF9:
		buffer="\e[20~";
		break;
	case kbF10:
		buffer="\e[21~";
		break;
	case kbF11:
		buffer="\e[23~";
		break;
	case kbF12:
		buffer="\e[24~";
		break;
	case kbAlA:
		buffer="\eA";
		break;
	case kbAlB:
		buffer="\eB";
		break;
	case kbAlC:
		buffer="\eC";
		break;
	case kbAlD:
		buffer="\eD";
		break;
	case kbAlE:
		buffer="\eE";
		break;
	case kbAlF:
		buffer="\eF";
		break;
	case kbAlG:
		buffer="\eG";
		break;
	case kbAlH:
		buffer = "\eH";
		break;
	case kbAlI:
		buffer="\eI";
		break;
	case kbAlJ:
		buffer="\eJ";
		break;
	case kbAlK:
		buffer="\eK";
		break;
	case kbAlL:
		buffer="\eL";
		break;
	case kbAlM:
		buffer="\eM";
		break;
	case kbAlN:
		buffer="\eN";
		break;
	case kbAlO:
		buffer="\eO";
		break;
	case kbAlP:
		buffer="\eP";
		break;
	case kbAlQ:
		buffer="\eQ";
		break;
	case kbAlR:
		buffer="\eR";
		break;
	case kbAlS:
		buffer="\eS";
		break;
	case kbAlT:
		buffer="\eT";
		break;
	case kbAlU:
		buffer="\eU";
		break;
	case kbAlV:
		buffer="\eV";
		break;
	case kbAlW:
		buffer="\eW";
		break;
	case kbAlX:
		buffer="\eX";
		break;
	case kbAlY:
		buffer="\eY";
		break;
	case kbAlZ:
		buffer="\eZ";
		break;
	case kbAl0:
		buffer="\e0";
		break;
	case kbAl1:
		buffer="\e1";
		break;
	case kbAl2:
		buffer="\e2";
		break;
	case kbAl3:
		buffer="\e3";
		break;
	case kbAl4:
		buffer="\e4";
		break;
	case kbAl5:
		buffer="\e5";
		break;
	case kbAl6:
		buffer="\e6";
		break;
	case kbAl7:
		buffer="\e7";
		break;
	case kbAl8:
		buffer="\e8";
		break;
	case kbAl9:
		buffer="\e9";
		break;
	default:
		break;
	}
	return buffer;
}

void TFelinks::createNewWindow()
{
	TRect r = deskTop->getExtent();
	//r.grow(-1, -1);

	TFelinksWindow *w = new TFelinksWindow(r, (char *)"felinks", 0);
	if (w)
	{
		deskTop->insert(w);
	}
}

void TFelinks::handleEvent(TEvent &event)
{
	TApplication::handleEvent(event);
	if (event.what == evCommand)
	{
		switch (event.message.command)
		{
		case cmNewWindow:
			createNewWindow();
			clearEvent(event);
			break;
		case cmHistory:
			fprintf(stderr, "cmHistory ");
			break;
		case cmUnHistory:
			fprintf(stderr, "cmUnHistory ");
			break;
		case cmKillBackgroundConnections:
			abort_background_connections();
			clearEvent(event);
			break;
		case cmFlushAllCaches:
			shrink_memory(1);
			clearEvent(event);
			break;
		case cmOSShell:
			fprintf(stderr, "cmOSShell ");
			break;
		case cmResizeTerminal:
			fprintf(stderr, "cmResizeTerminal ");
			break;


		case cmLinkSelected:
			fprintf(stderr, "cmLinkSelected ");
			break;

		case cmLanguage:
			fprintf(stderr, "cmLanguage ");
			break;
		case cmCharacterSet:
			fprintf(stderr, "cmCharacterSet ");
			break;


		case cmFileExtensions:
			fprintf(stderr, "cmFileExtensions ");
			break;
		case cmAbout:
			aboutDlgBox();
			break;
		case cmQuit2:
			if (messageBox("Do you really want to quit?", mfYesButton|mfNoButton|mfConfirmation) == cmYes)
			{
				TEvent e2 = event;
				e2.message.command = cmQuit;
				putEvent(e2);
			}
			break;
		default:
			break;
		}
		//clearEvent(event);
	}
	else if (event.what != 0)
	{
	}
}

TStatusLine* TFelinks::initStatusLine(TRect r)
{
	r.a.y = r.b.y - 1;
	return new TStatusLine(r,
	*new TStatusDef(0, 50) +
	*new TStatusItem(0, kbAlX, cmQuit) +
	*new TStatusItem(0, kbF9, cmMenu));
}

static char *tylda(const char *msgid)
{
	static char bufor[1024];
	int i;

	char *text = _(msgid);

	for (i = 0; *text && (i < 1023); ++i)
	{
		bufor[i] = *text++;
		if (bufor[i] == '~')
		{
			++i;
			bufor[i] = *text++;
			++i;
			bufor[i] = '~';
		}
	}
	bufor[i] = '\0';
	return bufor;
}

TMenuBar* TFelinks::initMenuBar(TRect r)
{
	r.b.y = r.a.y + 1;    // set bottom line 1 line below top line
	return new TMenuBar( r,
	*new TSubMenu( tylda("~File"), kbAlF )+
		*new TMenuItem( tylda("Open ~new window"), cmNewWindow, 0, hcNoContext, "" )+
		*new TMenuItem( tylda("Open new ~tab"),  cmMyNewTab,  0, hcNoContext, "t" ) +
		*new TMenuItem( tylda("Open new tab in backgroun~d"),  cmMyNewTabBackground,  0, hcNoContext, "" )+
		*new TMenuItem( tylda("~Go to URL"),  cmGotoUrl,  0, hcNoContext, "g" ) +
		*new TMenuItem( tylda("Go ~back"),  cmGoBack,  0, hcNoContext, "Left" ) +
		*new TMenuItem( tylda("Go ~forward"),  cmGoForward, 0, hcNoContext, "u" ) +
		*new TMenuItem( tylda("~History"),  cmHistory, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("~Unhistory"),  cmUnHistory, 0, hcNoContext, "" ) +
		newLine()+
		*new TMenuItem( tylda("~Save as"),  cmSaveAs, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("Save UR~L as"),  cmSaveUrlAs,  0, hcNoContext, "L" ) +
		*new TMenuItem( tylda("Sa~ve formatted document"),  cmSaveFormatted,  0, hcNoContext, "" ) +
		*new TMenuItem( tylda("Bookm~ark document"),  cmSaveFormatted, 0, hcNoContext, "a" ) +
		newLine()+
		*new TMenuItem( tylda("~Kill background connections"),  cmKillBackgroundConnections,  0, hcNoContext, "" ) +
		*new TMenuItem( tylda("Flush all ~caches"),  cmFlushAllCaches,  0, hcNoContext, "" ) +
		*new TMenuItem( tylda("Resource ~info"),  cmResourceInfo,  0, hcNoContext, "" ) +
		newLine()+
		*new TMenuItem( tylda("~OS shell"),  cmOSShell,  0, hcNoContext, "" ) +
		*new TMenuItem( tylda("Resize t~erminal"),  cmResizeTerminal,  0, hcNoContext, "" ) +
		newLine() +
		*new TMenuItem( tylda("E~xit"), cmQuit2, 0, hcNoContext, "q" )+
#if 1
	*new TSubMenu( tylda("~View"), kbAlV ) +
		*new TMenuItem( tylda("~Search"), cmSearch, 0, hcNoContext, "/" ) +
		*new TMenuItem( tylda("Search ~backward"), cmSearchBackward, 0, hcNoContext, "?" ) +
		*new TMenuItem( tylda("Find ~next"), cmFindNext, 0, hcNoContext, "n" ) +
		*new TMenuItem( tylda("Find ~previous"), cmFindPrevious, 0, hcNoContext, "N" ) +
		*new TMenuItem( tylda("T~ypeahead search"), cmTypeaheadSearch, 0, hcNoContext, "#" ) +
		newLine() +
		*new TMenuItem( tylda("Toggle ~HTML/plain"), cmToggleHtml, 0, hcNoContext, "\\" ) +
		*new TMenuItem( tylda("Toggle i~mages"), cmToggleImages, 0, hcNoContext, "*" ) +
		*new TMenuItem( tylda("Toggle ~link numbering"), cmToggleLinkNumbering, 0, hcNoContext, "." ) +
		*new TMenuItem( tylda("Toggle ~document colors"), cmToggleDocumentColors, 0, hcNoContext, "%" ) +
		*new TMenuItem( tylda("~Wrap text on/off"), cmToggleWrap, 0, hcNoContext, "w" ) +
		newLine() +
		*new TMenuItem( tylda("Document ~info"), cmDocumentInfo, 0, hcNoContext, "=" ) +
		*new TMenuItem( tylda("Rel~oad document"), cmReloadDocument, 0, hcNoContext, "Ctrl-R" ) +
		*new TMenuItem( tylda("~Rerender document"), cmRerenderDocument, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("Frame at ~full-screen"), cmFrameFullScreen, 0, hcNoContext, "f" ) +
		newLine() +
		*new TMenuItem( tylda("Nex~t tab"), cmNextTab, 0, hcNoContext, ">" ) +
		*new TMenuItem( tylda("Pre~v tab"), cmPrevTab, 0, hcNoContext, "<" ) +
		*new TMenuItem( tylda("~Close tab"), cmCloseTab, 0, hcNoContext, "c" ) +
	*new TSubMenu( tylda("~Link"), kbAlL ) +
		*new TMenuItem( tylda("No link selected"), cmLinkSelected, 0, hcNoContext, "" ) +
	*new TSubMenu( tylda("~Tools"), kbAlT ) +
		*new TMenuItem( tylda("Global ~history"), cmHistory2, 0, hcNoContext, "h" ) +
		*new TMenuItem( tylda("~Bookmarks"), cmBookmarks, 0, hcNoContext, "s" ) +
		*new TMenuItem( tylda("~Cache"), cmCache, 0, hcNoContext, "C" ) +
		*new TMenuItem( tylda("~Downloads"), cmDownloads, 0, hcNoContext, "D" ) +
		*new TMenuItem( tylda("Coo~kies"), cmCookies, 0, hcNoContext, "K" ) +
		*new TMenuItem( tylda("~Form history"), cmFormHistory, 0, hcNoContext, "F" ) +
		*new TMenuItem( tylda("~Authentication"), cmAuthentication, 0, hcNoContext, "" ) +
	*new TSubMenu( tylda("~Setup"), kbAlS ) +
		*new TMenuItem( tylda("~Language"), cmLanguage, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("C~haracter set"), cmCharacterSet, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("~Terminal options"), cmTerminalOptions, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("File ~extensions"), cmFileExtensions, 0, hcNoContext, "" ) +
		newLine() +
		*new TMenuItem( tylda("~Options manager"), cmOptionsManager, 0, hcNoContext, "O" ) +
		*new TMenuItem( tylda("Keybinding manager"), cmKeybindingManager, 0, hcNoContext, "k" ) +
		*new TMenuItem( tylda("~Save options"), cmSaveOptions, 0, hcNoContext, "" ) +
	*new TSubMenu( tylda("~Help"), kbAlH ) +
		*new TMenuItem( "F~e~links homepage", cmHomePage, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("~Documentation"), cmDocumentation, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("~Keys"), cmKeys, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("LED ~indicators"), cmLedIndicators, 0, hcNoContext, "" ) +
		newLine() +
		*new TMenuItem( tylda("~Bugs information"), cmBugs, 0, hcNoContext, "" ) +
		newLine() +
		*new TMenuItem( tylda("~Copying"), cmCopying, 0, hcNoContext, "" ) +
		*new TMenuItem( tylda("Autho~rs"), cmAuthors, 0, hcNoContext, "" ) +
#endif
		*new TMenuItem( tylda("~About"), cmAbout, 0, hcNoContext, "" )
	);
}

TTermScrollBar *TFelinksWindow::standardScrollBar2(unsigned short aOptions)
{
	TRect r = getExtent();
	if ((aOptions & sbVertical) != 0)
	{
		r = TRect(r.b.x-1, r.a.y+1, r.b.x, r.b.y-1);
	}
	else
	{
		r = TRect(r.a.x+2, r.b.y-1, r.b.x-2, r.b.y);
	}
	TTermScrollBar *s;
	insert(s = new TTermScrollBar(r));
	if ((aOptions & sbHandleKeyboard) != 0)
	{
		s->options |= ofPostProcess;
	}
	return s;
}

static unsigned short decode_color(unsigned short c)
{
	unsigned short fg = c & 7;
	unsigned short bg = (c >> 4) & 7;
	unsigned short bold = c & 8;

	return bold | color_values[fg] | (color_values[bg] << 4);
}

static const unsigned char frame_vt100[48] =	"aaaxuuukkuxkjjjkmvwtqnttmlvwtqnvvwwmmllnnjla    ";

#if 0


static const unsigned char frame_vt100[48] = {
	177, 177, 177, 179, 180, 180, 180, 191,
	191, 180, 179, 191, 217, 217, 217, 191,
	192, 193, 194, 195, 196, 197, 195, 195,
	192, 218, 193, 194, 195, 196, 197, 193,
	193, 194, 194, 192, 192, 218, 218, 197,
	197, 217, 218, 177,  32, 32,  32,  32
};
#endif

static unsigned short decode_frame(struct screen_char *c)
{
	if (!(c->attr & SCREEN_ATTR_FRAME))
	{
		return remap_letter[c->data];
	}

	if (c->data >= 176 && c->data < 224)
	{
		return tv_frames(frame_vt100[c->data - 176]);
	}
	return c->data;
}

/*! Updating of the terminal screen is done by checking what needs to
 * be updated using the last screen. */
void
redraw_screen(struct terminal *term)
{

	struct terminal_screen *screen = term->screen;
	TTermView *fwin = (TTermView *)term->fWindow;
	struct screen_char *c;
	int y;

	//fprintf(stderr, "redraw_screen: term = %p fwin=%p from=%d to=%d\n", term, fwin, screen->dirty_from, screen->dirty_to);

	if (!screen || !fwin || screen->dirty_from > screen->dirty_to) return;

	for (c = screen->image, y = 0; y < term->height; ++y)
	{
		TDrawBuffer d;
		for (int x = 0; x < term->width; ++x)
		{
			d.putAttribute(x, decode_color(c->c.color[0]));
			d.putChar(x, decode_frame(c));
			++c;
		}
		fwin->writeLine(0, y, term->width, 1, d);
	}
	copy_screen_chars(screen->last_image, screen->image, term->width * term->height);
	screen->dirty_from = term->height;
	screen->dirty_to = 0;

	fwin->draw();
	fwin->setCursor(screen->cx, screen->cy);
	fwin->showCursor();
}

int thread = 1;
pthread_t pth;

TFelinksWindow::TFelinksWindow(TRect& bounds, char *str, int num):
TWindowInit(&TWindow::initFrame), TWindow(bounds, str, num)
{
	eventMask = evMouseDown | evMouseUp | evKeyDown | evCommand | evBroadcast;

	TRect r(getExtent());
	r.grow(-1, -1);
	TTermScrollBar *pion = standardScrollBar2(sbVertical);
	TTermScrollBar *poziom = standardScrollBar2(sbHorizontal);

	tw = new TTermView(r, this, pion, poziom);
	if (tw)
	{
		insert(tw);
	}
	init_next();
	if (thread)
	{
		thread = pthread_create(&pth, NULL, select_loop, NULL);
	}

}

TFelinksWindow::~TFelinksWindow()
{
}

void TFelinksWindow::handleEvent(TEvent& event)
{
	TWindow::handleEvent(event);
}

void TFelinksWindow::setTitle(char *text)
{
	DeleteArray((char *)title);
	TVIntl::freeSt(intlTitle);
	title = newStr(text);
}

void TFelinksWindow::dragView(TEvent& event, uchar mode, TRect& limits, TPoint minSize, TPoint maxSize)
{
	TWindow::dragView(event, mode, limits, minSize, maxSize);
	tw->zmienRozmiar(size);
}

void TFelinksWindow::zoom()
{
	TWindow::zoom();
	tw->zmienRozmiar(size);
}

int terminalLines;
int terminalColumns;

TTermView::TTermView(TRect &r, TFelinksWindow *p, TTermScrollBar *pion1, TTermScrollBar *poziom1) : TView(r), pWindow(p),
pion(pion1), poziom(poziom1)
{
	eventMask |= evMouseUp;
	options = ofSelectable;
	pion->pWindow = this;
	poziom->pWindow = this;

	terminalLines = r.b.y - r.a.y;
	terminalColumns = r.b.x - r.a.x;

	currentFWindow = (void *)this;
	initialized = true;
}

TTermView::~TTermView()
{
	if (term)
	{
		term->fWindow = NULL;
		destroy_terminal(term);
	}
}

void TTermView::zmienRozmiar(TPoint &s)
{
	terminalLines = s.y - 2;
	terminalColumns = s.x - 2;
	growTo(terminalColumns, terminalLines);
	drawView();
	resizeTerminal(itrm);
}

void TTermView::draw()
{
	return;
	if (pion)
	{
		pion->draw();
	}
	if (poziom)
	{
		poziom->draw();
	}
}

void setTerminalTitle(struct terminal *term, unsigned char *text)
{
	if (!term->fWindow) return;
	if (!text) return;
	TTermView *view = (TTermView *)term->fWindow;
	if (view->pWindow)
	{
		view->pWindow->setTitle((char *)text);
		view->pWindow->frame->draw();
	}
}

static void sendSequence(struct itrm *itrm, char *seq)
{
	int len = strlen(seq);

	kill_timer(&itrm->timer);

	if (itrm->in.queue.len >= ITRM_IN_QUEUE_SIZE)
	{
		unhandle_itrm_stdin(itrm);
		while (process_queue(itrm));
		return;
	}

	if (len > (ITRM_IN_QUEUE_SIZE - itrm->in.queue.len))
	{
		return;
	}

	memcpy(itrm->in.queue.data + itrm->in.queue.len, seq, len);

	itrm->in.queue.len += len;
	if (itrm->in.queue.len > ITRM_IN_QUEUE_SIZE)
	{
		ERROR((const char *)gettext("Too many bytes read from the itrm!"));
		itrm->in.queue.len = ITRM_IN_QUEUE_SIZE;
	}

	while (process_queue(itrm));
}

char *TTermView::getMouseSequence(TEvent &event)
{
	static char buffer[7];
	unsigned char b = 3;
	TPoint l = makeLocal(event.mouse.where);

	if (event.mouse.buttons & 1) b = 0;
	else if (event.mouse.buttons & 2) b = 1;
	else if (event.mouse.buttons & 4) b = 2;
	else if (event.mouse.buttons & 8) b = 96-32;
	else if (event.mouse.buttons & 16) b = 97-32;
	else if ((event.mouse.buttons & 7) == 0) b = 3;

	buffer[0] = '\033';
	buffer[1] = '[';
	buffer[2] = 'M';
	buffer[3] = b + ' ';
	buffer[4] = l.x + ' ' + 1;
	buffer[5] = l.y + ' ' + 1;
	buffer[6] = 0;
	return buffer;
}

void TTermView::handleEvent(TEvent &event)
{
	char *seq;

	TView::handleEvent(event);
	switch (event.what)
	{
	case evKeyDown:
		seq = getKeySequence(event);
		if (itrm && seq)
		{
			sendSequence(itrm, seq);
		}
		clearEvent(event);
		break;
	case evMouseUp:
	case evMouseDown:
		seq = getMouseSequence(event);

		if (itrm)
		{
			sendSequence(itrm, seq);
		}
		clearEvent(event);
		break;
	case evCommand:
		switch (event.message.command)
		{
		case cmMyNewTab:
			if (ses) open_uri_in_new_tab(ses, NULL, 0, 1);
			clearEvent(event);
			break;
		case cmMyNewTabBackground:
			if (ses) open_uri_in_new_tab(ses, NULL, 1, 1);
			clearEvent(event);
			break;
		case cmGotoUrl:
			if (ses) goto_url_action(ses, NULL);
			clearEvent(event);
			break;
		case cmGoBack:
			if (ses) go_history_by_n(ses, -1);
			clearEvent(event);
			break;
		case cmGoForward:
			if (ses) go_history_by_n(ses, 1);
			clearEvent(event);
			break;
		case cmSaveAs:
			if (ses && doc_view)
			{
				save_as(ses, doc_view, 0);
			}
			clearEvent(event);
		case cmSaveUrlAs:
			if (ses) save_url_as(ses);
			clearEvent(event);
			break;
		case cmSaveFormatted:
			if (ses && doc_view)
			{
				save_formatted_dlg(ses, doc_view, 0);
			}
			clearEvent(event);
			break;
		case cmResourceInfo:
			if (term) resource_info(term);
			clearEvent(event);
			break;
		case cmSearch:
			if (ses && doc_view)
			{
				search_dlg(ses, doc_view, 1);
			}
			clearEvent(event);
			break;
		case cmSearchBackward:
			if (ses && doc_view)
			{
				search_dlg(ses, doc_view, -1);
			}
			clearEvent(event);
			break;
		case cmFindNext:
			if (ses && doc_view)
			{
				find_next(ses, doc_view, 1);
			}
			clearEvent(event);
			break;
		case cmFindPrevious:
			if (ses && doc_view)
			{
				find_next(ses, doc_view, -1);
			}
			clearEvent(event);
			break;
		case cmTypeaheadSearch:
			if (ses && doc_view)
			{
				search_typeahead(ses, doc_view, ACT_MAIN_SEARCH_TYPEAHEAD);
			}
			clearEvent(event);
			break;
		case cmToggleHtml:
			if (ses)
			{
				toggle_plain_html(ses, ses->doc_view, 0);
			}
			clearEvent(event);
			break;
		case cmToggleImages:
			if (ses)
			{
				toggle_document_option(ses, "document.browse.images.show_as_links");
			}
			clearEvent(event);
			break;
		case cmToggleLinkNumbering:
			if (ses)
			{
				toggle_document_option(ses, "document.browse.links.numbering");
			}
			clearEvent(event);
			break;
		case cmToggleDocumentColors:
			if (ses)
			{
				toggle_document_option(ses, "document.colors.use_document_colors");
			}
			clearEvent(event);
			break;
		case cmToggleWrap:
			if (ses)
			{
				toggle_wrap_text(ses, ses->doc_view, 0);
			}
			clearEvent(event);
			break;
		case cmDocumentInfo:
			if (ses)
			{
				document_info_dialog(ses);
			}
			clearEvent(event);
			break;
		case cmReloadDocument:
			if (ses)
			{
				reload(ses, CACHE_MODE_INCREMENT);
			}
			clearEvent(event);
			break;
		case cmRerenderDocument:
			if (ses)
			{
				draw_formatted(ses, 2);
			}
			clearEvent(event);
			break;
		case cmFrameFullScreen:
			if (ses && doc_view)
			{
				set_frame(ses, doc_view, 0);
			}
			clearEvent(event);
			break;
		case cmNextTab:
			if (ses)
			{
				switch_current_tab(ses, 1);
			}
			clearEvent(event);
			break;
		case cmPrevTab:
			if (ses)
			{
				switch_current_tab(ses, -1);
			}
			clearEvent(event);
			break;
		case cmCloseTab:
			if (ses && term)
			{
				close_tab(term, ses);
			}
			clearEvent(event);
			break;
		case cmHistory2:
			if (ses)
			{
				history_manager(ses);
			}
			clearEvent(event);
			break;
		case cmBookmarks:
			if (ses)
			{
				bookmark_manager(ses);
			}
			clearEvent(event);
			break;
		case cmCache:
			if (ses)
			{
				cache_manager(ses);
			}
			clearEvent(event);
			break;
		case cmDownloads:
			if (ses)
			{
				download_manager(ses);
			}
			clearEvent(event);
			break;
		case cmCookies:
			if (ses)
			{
				cookie_manager(ses);
			}
			clearEvent(event);
			break;
		case cmFormHistory:
			if (ses)
			{
				formhist_manager(ses);
			}
			clearEvent(event);
			break;
		case cmAuthentication:
			if (ses)
			{
				auth_manager(ses);
			}
			clearEvent(event);
			break;
		case cmTerminalOptions:
			if (ses && term)
			{
				terminal_options(term, NULL, ses);
			}
			clearEvent(event);
			break;
		case cmOptionsManager:
			if (ses)
			{
				options_manager(ses);
			}
			clearEvent(event);
			break;
		case cmKeybindingManager:
			if (ses)
			{
				keybinding_manager(ses);
			}
			clearEvent(event);
			break;
		case cmSaveOptions:
			if (term)
			{
				write_config(term);
			}
			clearEvent(event);
			break;
		case cmHomePage:
			if (ses && term)
			{
				menu_url_shortcut(term, ELINKS_WEBSITE_URL, ses);
			}
			clearEvent(event);
			break;
		case cmDocumentation:
			if (ses && term)
			{
				menu_url_shortcut(term, ELINKS_DOC_URL, ses);
			}
			clearEvent(event);
			break;
		case cmKeys:
			if (term)
			{
				menu_keys(term, NULL, NULL);
			}
			clearEvent(event);
			break;
		case cmLedIndicators:
			if (term)
			{
				menu_leds_info(term, NULL, NULL);
			}
			clearEvent(event);
			break;
		case cmBugs:
			if (ses && term)
			{
				menu_url_shortcut(term, ELINKS_BUGS_URL, ses);
			}
			clearEvent(event);
			break;
		case cmCopying:
			if (term)
			{
				menu_copying(term, NULL, NULL);
			}
			clearEvent(event);
			break;
		case cmAuthors:
			if (ses && term)
			{
				menu_url_shortcut(term, ELINKS_AUTHORS_URL, ses);
			}
			clearEvent(event);
			break;
		default:
			break;
		}
	default:
		break;
	}
}

void setTermItrm(struct terminal *term, struct itrm *itrm)
{
	if (!term || !term->fWindow || !itrm) return;
	TTermView *tv = (TTermView *)term->fWindow;
	tv->itrm = itrm;
}

static void init_next(void)
{
	struct itrm *itrm;
	int fd = init_interlink();

	if (fd == -1)
	{
		check_terminal_pipes();

		load_config();
		update_options_visibility();
		/* Parse commandline options again, in order to override any
		 * config file options. */
		parse_options(ac - 1, av + 1, NULL);
		/* ... and re-check stdio, in order to override any command
		 * line options! >;) */

		//init_b = 1;
		init_modules(builtin_modules);

		struct terminal *term = attach_terminal(-1, -1, -1, "", 0);

		if (!term)
		{
			ERROR((const char *)gettext("Unable to attach_terminal()."));
			program.retval = RET_FATAL;
			program.terminate = 1;
		}
		return;
	}

	itrm = handle_trm(-1, -1, fd, fd, -1, "", 0, 0);
	TTermView *tv = (TTermView *)currentFWindow;
	if (!tv) return;
	tv->itrm = itrm;
}

void connectTerminalToWindow(struct terminal *term)
{
	TTermView *tv = (TTermView *)currentFWindow;
	if (!tv) return;
	tv->term = term;
	term->fWindow = tv;
}

void TTermView::changePion(int ny)
{
	vertical_scroll(ses, doc_view, ny - doc_view->vs->y);
	refresh_view(ses, doc_view, 1);
}

void TTermView::changePoziom(int nx)
{
	horizontal_scroll(ses, doc_view, nx - doc_view->vs->x);
	refresh_view(ses, doc_view, 1);
}

TTermScrollBar::TTermScrollBar(const TRect &r) : TScrollBar(r)
{
}

TTermScrollBar::~TTermScrollBar()
{
}

void TTermScrollBar::handleEvent(TEvent &event)
{
	int oldvalue = value;
	bool mouse = (event.what == evMouseDown);

	TScrollBar::handleEvent(event);
	if (mouse && (oldvalue != value))
	{
		if (this == pWindow->pion)
		{
			pWindow->changePion(value);
		}
		else if (this == pWindow->poziom)
		{
			pWindow->changePoziom(value);
		}
	}
}

void setSessionTerm(struct session *ses, struct terminal *term)
{
	TTermView *tv = (TTermView *)term->fWindow;

	if (tv) tv->ses = ses;
}

void refreshScrollBars(struct session *ses, struct document_view *doc_view)
//, int x, int y, int width, int height, int box_width, int box_height)
{
	//fprintf(stderr, "refreshScrollBars: ses=%p\n", ses);

	if (!ses || !ses->tab || !ses->tab->term || !ses->tab->term->fWindow) return;
	TTermView *tv = (TTermView *)ses->tab->term->fWindow;
	tv->ses = ses;

	if (!doc_view || !doc_view->document || !doc_view->vs) return;

	tv->doc_view = doc_view;
	if (tv->pion)
	{
		tv->pion->setParams(doc_view->vs->y, 0, doc_view->document->height - doc_view->box.height, doc_view->box.height, 1);
	}
	if (tv->poziom)
	{
		tv->poziom->setParams(doc_view->vs->x, 0, doc_view->document->width - 1, doc_view->box.width, 1);
	}
}

void exit_prog(struct session *ses, int ask)
{
	if (ask)
	{
		message(mainFelinks, evCommand, cmQuit2, NULL);
	}
	else
	{
		message(mainFelinks, evCommand, cmQuit, NULL);
	}
}

int main(int argc, char **argv)
{
	char *term = getenv("TERM");
	setlocale(LC_ALL,"");

	if (term && !strcmp(term, "linux"))
	{
		char *codeset = nl_langinfo(CODESET);

		if (codeset && !strcmp(codeset, "ISO-8859-2"))
		{
			linux_console_8859_2 = true;
		}
	}

	if (getenv("DISPLAY"))
	{
#ifdef HAVE_X11
		XInitThreads();
#endif
		linux_console_8859_2 = false;
	}
	setenv("TERM", "linux", 1);

	init_remap_chars();
	ac = argc;
	av = argv;

	if (linux_console_8859_2) TFelinks::oldCPCallBack = TVCodePage::SetCallBack(TFelinks::cpCallBack);
	BINDTEXTDOMAIN("elinks", LOCALEDIR);
	TEXTDOMAIN("elinks");
	main2(argc, argv);
	select_loop_prepare();
	TFelinks app;
	app.run();
	return 0;
}

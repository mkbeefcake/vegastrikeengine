#ifndef __GAMEMENU_H__
#define __GAMEMENU_H__

#include "gui/windowcontroller.h"
#include "gui/simplepicker.h"

class GameMenu : public WctlBase<GameMenu> {
	friend class WctlBase<GameMenu>;
public:
	virtual void init(void);
	virtual void run(void);

	GameMenu(bool firsttime);
	virtual ~GameMenu(void);
	
	static void startMenuInterface(bool firsttime);
	
protected:
	bool m_firstTime;
	
	bool processSinglePlayerButton(const EventCommandId& command, Control *control);
	bool processMultiPlayerHostButton(const EventCommandId& command, Control *control);
	bool processMultiPlayerAcctButton(const EventCommandId& command, Control *control);
	bool processExitGameButton(const EventCommandId& command, Control *control);
	bool processJoinGameButton(const EventCommandId& command, Control *control);
	bool processMainMenuButton(const EventCommandId& command, Control *control);
	
	void createControls();
};	

// Helper functions for use in a submenu of BaseComputer.

// Caller is responsible for making a return button, as well as adding the "JoinGame" WctlTableEntry
void createNetworkControls(GroupControl *serverConnGroup, std::vector <unsigned int> *keyboard_input_queue);

// Caller is responsible for closing the window afterwards.
void processJoinGame(Window *window, bool firstTime, string &user, string &pass);

#endif
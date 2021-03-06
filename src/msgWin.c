#include "msgWin.h"


/**
 * Creates the message window, the one on the bottom that shows messages to the
 * user, and also is where the user types input.
 */
WINDOW *createMsgWin(AtcsoData *data) {
    WINDOW *msgWin = newwin(3, 80, 21, 0);

    updateMsgWin(data, msgWin);

    return msgWin;
}


/**
 * Updates and refreshes the message window.
 */
bool updateMsgWin(AtcsoData *data, WINDOW *msgWin) {
    // nothing to do here...

    wrefresh(msgWin);

    return false;
}

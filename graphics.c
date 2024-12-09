/**
 * References for programming with 'curses'
 * 
 * Curses Library Guide - https://www.ibm.com/docs/en/aix/7.1?topic=concepts-curses-library
 * Charlie's worm lab - https://curtsinger.cs.grinnell.edu/teaching/2024F/CSC213/labs/worm/
 * 
 */

#include "graphics.h"

/**
 * Initializes the curses environment
 */
void init_curses() {
    initscr();           // Initialize the screen
    cbreak();            // Disable line buffering
    noecho();            // Disable echoing of input
    curs_set(1);         // Show the cursor
    keypad(stdscr, TRUE); // Enable special keys (e.g., arrows)
    mousemask(0, NULL);
}

/**
 * Creates a new window to represent a player's or opponent's game board.
 * 
 * @param start_x  The starting column position for the window.
 * @param start_y  The starting row position for the window.
 * @param title    The title to display at the top of the window.
 * 
 * @return A pointer to the created window.
 */
WINDOW* create_board_window(int start_x, int start_y, const char* title) {
    WINDOW* win = newwin(15, 35, start_x, start_y); // 10x10 grid + padding
    box(win, 0, 0);                                // Draw a border around the window
    mvwprintw(win, 0, 2, "[ %s ]", title);         // Add a title to the window
    // move(16, 0);
    wrefresh(win);                                 // Refresh the window to display it
    return win;
}

/**
 * Draws the game board for a player or opponent in the given window.
 * 
 * @param win        The window where the board will be drawn.
 * @param board      The 10x10 game board array to display.
 * @param hide_ships If true, hides the ships from view (used for opponent's board).
 */
void draw_board(WINDOW* win, cell_t board[NROWS+1][NCOLS+1], bool hide_ships) {
    for (int x = 1; x < NROWS + 1; x++) {
        for (int y = 1; y < NCOLS + 1; y++) {
            char symbol = '~'; // Default empty cell
            if (board[x][y].guessed) {
                symbol = board[x][y].hit ? 'H' : 'M'; // Hit or Miss
            } else if (!hide_ships && board[x][y].occupied) {
                symbol = 'S'; // Display ship if not hidden
            }
            mvwprintw(win, x + 1, y * 2, "%c", symbol); // Adjust cell spacing
        }
    }
    wrefresh(win);
}

/**
 * Creates a new window to display prompts and handle user input.
 * 
 * @param start_x  The starting column position for the window.
 * @param start_y  The starting row position for the window.
 * 
 * @return A pointer to the created window.
 */
WINDOW* create_prompt_window(int start_x, int start_y) {
    WINDOW* win = newwin(30, 74, start_x, start_y);
    box(win, 0, 0);
    mvwprintw(win, 0, 2, "[ Prompt ]");
    wrefresh(win);
    return win;
}

/**
 * Displays a prompt message in the given window and clears the input field.
 * 
 * @param win     The window where the prompt is displayed.
 * @param message The message to display.
 */
void display_prompt(WINDOW* win, const char* prompt, char* input, int max_len) {
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 0, 2, "[ Prompt ]");
    mvwprintw(win, 2, 2, "%s", prompt);
    wrefresh(win);

    // Get user input
    echo();                                   // Enable input echoing
    curs_set(1);                              // Show the cursor
    mvwgetnstr(win, 3, 2, input, max_len);    // Get the user's input
    noecho();                                 // Disable input echoing
    curs_set(0);     
}

/**
 * End the curses environment
 */
void end_curses() {
    endwin(); // End curses mode
}
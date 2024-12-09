/**
 * File basis taken from Charlie's networking exercise, and refined for our context - https://curtsinger.cs.grinnell.edu/teaching/2024F/CSC213/exercises/networking/
 */

#include "battleship.h"

int main(int argc, char *argv[]) {
    // Validate command-line arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <role> [<server_name> <port>]\n", argv[0]);
        fprintf(stderr, "Role: server or client\n");
        exit(EXIT_FAILURE);
    }

    // Check if the user wants to start as a server
    if (strcmp(argv[1], "server") == 0) {
        unsigned short port = 0;    // Initialize the port
        printf("Starting server...\n");
        run_server(port);
    } 
    // Check if the user wants to start as a client
    else if (strcmp(argv[1], "client") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage for client: %s client <server_name> <port>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
        char *server_name = argv[2];
        unsigned short port = atoi(argv[3]);
        printf("Connecting to server %s on port %u...\n", server_name, port);
        run_client(server_name, port);
    } 
    // Invalid role provided
    else {
        fprintf(stderr, "Invalid role. Use 'server' or 'client'.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/**
 * Initializes the server-side (Player 1) logic for the game 
 * 
 * @param port The port number the server will listen on
 */ 
void run_server(unsigned short port) {
    // Create and bind the server socket
    int server_socket_fd = server_socket_open(&port);
    if (server_socket_fd == -1) {
        perror("Failed to open server socket");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    printf("Server listening on port %u\n", port);
    if (listen(server_socket_fd, 1) == -1) {
        perror("Failed to listen on server socket");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }

    // Accept a client connection
    int client_socket_fd = server_socket_accept(server_socket_fd);
    if (client_socket_fd == -1) {
        perror("Failed to accept client connection");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("Player 2 connected!\n");
    sleep(1);
    printf("Game Loading. Please fullscreen your terminal."); 
    sleep(5);

    // Initialize curses for graphics
    init_curses();

    // Create graphical windows for the boards
    WINDOW* player_win = create_board_window(1, 1, "Your Board");
    WINDOW* opponent_win = create_board_window(1, 40, "Opponent's Board");
    WINDOW* prompt_win = create_prompt_window(16, 1);

    // Display welcome message
    welcome_message(prompt_win);

    // Initialize game boards for both players
    board_t player1_board, player2_board;
    // initBoard(&player1_board);
    // initBoard(&player2_board);

    // Player 1 places ships
    mvwprintw(prompt_win, 1, 1, "**Place your ships**");
    wrefresh(prompt_win);
    player1_board = makeBoard(prompt_win, player_win);
    // memcpy(&player1_board, &(makeBoard(prompt_win)), ((sizeof(cell_t))*(NROWS+1)*(NCOLS+1)));

    // Update the player's board window
    draw_board(player_win, player1_board.array, false);

    // Notify the client that the server is ready
    send_message(client_socket_fd, "READY");

    // Wait for the client to be ready
    char* message = receive_message(client_socket_fd);
    if (strcmp(message, "READY") != 0) {
        printf("Client not ready. Exiting.\n");
        free(message);
        close(client_socket_fd);
        close(server_socket_fd);
        end_curses();
        exit(EXIT_FAILURE);
    }
    free(message);

    // Main game loop
    bool game_running = true;
    while (game_running) {
        int attack_coords[2];
        int x, y;

        // Player 1's turn
        mvwprintw(prompt_win, 1, 1, "Enter attack coordinates (e.g., A,1): ");
        wrefresh(prompt_win);
        validCoords(attack_coords, prompt_win);
        x = attack_coords[0];  // Row index
        y = attack_coords[1];  // Column index

        // Send attack to Player 2
        char attack_message[16];
        snprintf(attack_message, sizeof(attack_message), "%d,%d", x, y);
        send_message(client_socket_fd, attack_message);

        // Receive attack result
        char* attack_result = receive_message(client_socket_fd);
        mvwprintw(prompt_win, 1, 1, "Player 2: %s\n", attack_result);
        wrefresh(prompt_win);

        // Update the opponent's board window with the result
        if (strcmp(attack_result, "HIT") == 0) {
            player2_board.array[x][y].guessed = true;
            player2_board.array[x][y].hit = true;
        } else if (strcmp(attack_result, "MISS") == 0) {
            player2_board.array[x][y].guessed = true;
        }
        draw_board(opponent_win, player2_board.array, true);
        free(attack_result);

        // Check if Player 1 won
        if (checkVictory(&player2_board)) {
            mvwprintw(prompt_win, 1, 1, "Player 1 wins!\n");
            wrefresh(prompt_win);
            send_message(client_socket_fd, "WIN");
            break;
        }

        // Player 2's turn
        mvwprintw(prompt_win, 1, 1, "Waiting for Player 2's attack...\n");
        wrefresh(prompt_win);
        char* enemy_attack = receive_message(client_socket_fd);
        sscanf(enemy_attack, "%d,%d", &attack_coords[0], &attack_coords[1]);
        x = attack_coords[0];
        y = attack_coords[1];
        free(enemy_attack);

        // Update Player 1's board based on Player 2's attack
        bool hit, sunk;
        updateBoardAfterGuess(&player1_board, x, y, &hit, &sunk, prompt_win);

        // Send attack result to Player 2
        char result_message[16];
        snprintf(result_message, sizeof(result_message), "%s%s", hit ? "HIT" : "MISS", sunk ? " (sunk)" : "");
        send_message(client_socket_fd, result_message);

        // Update the player's board window with the result
        draw_board(player_win, player1_board.array, false);

        // Check if Player 2 won
        if (checkVictory(&player1_board)) {
            mvwprintw(prompt_win, 1, 1, "Player 2 wins!\n");
            wrefresh(prompt_win);
            send_message(client_socket_fd, "LOSE");
            break;
        }
    }

    // Close sockets and end curses
    close(client_socket_fd);
    close(server_socket_fd);
    end_curses();
}


/**
 * Initializes the client-side (Player 2) logic for the game
 * 
 * @param server_name The IP or hostname of the server
 * @param port        The port number the server is listening on.
 */
void run_client(char* server_name, unsigned short port) {
    // Connect to the server
    int socket_fd = socket_connect(server_name, port);
    if (socket_fd == -1) {
        perror("Failed to connect to server");
        exit(EXIT_FAILURE);
    }

    printf("Connected to Player 1!\n");
    sleep(1);
    printf("Game Loading. Please fullscreen your terminal."); 
    sleep(5);

    // Initialize curses for graphics
    init_curses();

    // Create graphical windows for the boards
    WINDOW* player_win = create_board_window(1, 1, "Your Board");
    WINDOW* opponent_win = create_board_window(1, 40, "Opponent's Board");
    WINDOW* prompt_win = create_prompt_window(16, 1);

    // Display welcome message
    welcome_message(prompt_win);

    // Initialize game boards for both players
    board_t player2_board, player1_board;
    initBoard(&player2_board);
    initBoard(&player1_board);

    // Player 2 places ships
    mvwprintw(prompt_win, 1, 1, "**Place your ships**");
    wrefresh(prompt_win);
    // memcpy(&player2_board, &(makeBoard(prompt_win)), ((sizeof(cell_t))*(NROWS+1)*(NCOLS+1)));
    player2_board = makeBoard(prompt_win, player_win);

    // Update the player's board window
    draw_board(player_win, player2_board.array, false);

    // Notify the server that the client is ready
    send_message(socket_fd, "READY");

    // Wait for the server to be ready
    char* message = receive_message(socket_fd);
    if (strcmp(message, "READY") != 0) {
        mvwprintw(prompt_win, 1, 1, "Server not ready. Exiting.\n");
        wrefresh(prompt_win);
        free(message);
        close(socket_fd);
        end_curses();
        exit(EXIT_FAILURE);
    }
    free(message);

    // Main game loop
    bool game_running = true;
    while (game_running) {
        int attack_coords[2];
        int x, y;

        // Player 1's turn
        mvwprintw(prompt_win, 1, 1, "Waiting for Player 1's attack...\n");
        wrefresh(prompt_win);
        char* enemy_attack = receive_message(socket_fd);
        sscanf(enemy_attack, "%d,%d", &attack_coords[0], &attack_coords[1]);
        x = attack_coords[0];
        y = attack_coords[1];
        free(enemy_attack);

        // Update Player 2's board based on Player 1's attack
        bool hit, sunk;
        updateBoardAfterGuess(&player2_board, x, y, &hit, &sunk, prompt_win);

        // Send attack result to Player 1
        char result_message[16];
        snprintf(result_message, sizeof(result_message), "%s%s", hit ? "HIT" : "MISS", sunk ? " (sunk)" : "");
        send_message(socket_fd, result_message);

        // Update the player's board window with the result
        draw_board(player_win, player2_board.array, false);

        // Check if Player 1 won
        if (checkVictory(&player2_board)) {
            mvwprintw(prompt_win, 1, 1, "Player 1 wins!\n");
            wrefresh(prompt_win);
            send_message(socket_fd, "LOSE");
            break;
        }

        // Player 2's turn
        mvwprintw(prompt_win, 1, 1, "**Enter attack coordinates (e.g., A,1): ");
        wrefresh(prompt_win);
        validCoords(attack_coords, prompt_win);
        x = attack_coords[0];
        y = attack_coords[1];

        // Send attack to Player 1
        char attack_message[16];
        snprintf(attack_message, sizeof(attack_message), "%d,%d", x, y);
        send_message(socket_fd, attack_message);

        // Receive attack result
        char* attack_result = receive_message(socket_fd);
        mvwprintw(prompt_win, 1, 1, "Player 1: %s\n", attack_result);
        wrefresh(prompt_win);

        // Update the opponent's board window with the result
        if (strcmp(attack_result, "HIT") == 0) {
            player1_board.array[x][y].guessed = true;
            player1_board.array[x][y].hit = true;
        } else if (strcmp(attack_result, "MISS") == 0) {
            player1_board.array[x][y].guessed = true;
        }
        draw_board(opponent_win, player1_board.array, true);
        free(attack_result);

        // Check if Player 2 won
        if (checkVictory(&player1_board)) {
            mvwprintw(prompt_win, 1, 1, "Player 2 wins!\n");
            wrefresh(prompt_win);
            send_message(socket_fd, "WIN");
            break;
        }
    }

    // Close the connection and end curses
    close(socket_fd);
    end_curses();
}


/**
 * Display a welcome message to the players when they connect to the server.
 * 
 * @param prompt_win The curses window for displaying prompt
 */
void welcome_message(WINDOW* prompt_win) {
    wclear(prompt_win);     // Clear the prompt window
    box(prompt_win, 0, 0);  // Redraw the border
    mvwprintw(prompt_win, 1, 1, "============================================================");
    mvwprintw(prompt_win, 2, 1, "                WELCOME TO BATTLESHIP!");
    mvwprintw(prompt_win, 3, 1, "============================================================");
    mvwprintw(prompt_win, 5, 1, "Rules of the game:");
    mvwprintw(prompt_win, 6, 1, "1. Each player has a 10x10 grid to place 5 ships:");
    mvwprintw(prompt_win, 7, 1, "   - Destroyer (2 spaces)");
    mvwprintw(prompt_win, 8, 1, "   - Submarine (3 spaces)");
    mvwprintw(prompt_win, 9, 1, "   - Cruiser (3 spaces)");
    mvwprintw(prompt_win, 10, 1, "   - Battleship (4 spaces)");
    mvwprintw(prompt_win, 11, 1, "   - Aircraft Carrier (5 spaces)");
    mvwprintw(prompt_win, 13, 1, "2. Players take turns guessing coordinates to attack.");
    mvwprintw(prompt_win, 14, 1, "3. A hit will mark part of a ship as damaged.");
    mvwprintw(prompt_win, 15, 1, "   NOTE: Players do not get consecutive turns if they hit an enemy ship");
    mvwprintw(prompt_win, 16, 1, "4. A ship is sunk when all its parts are hit.");
    mvwprintw(prompt_win, 17, 1, "5. The game ends when all ships of one player are sunk.");
    mvwprintw(prompt_win, 19, 1, "Type 'exit' or 'quit' anytime to leave the game.");
    mvwprintw(prompt_win, 20, 1, "============================================================");
    mvwprintw(prompt_win, 21, 1, "                 LET THE BATTLE BEGIN!");
    mvwprintw(prompt_win, 22, 1, "============================================================");
    mvwprintw(prompt_win, 24, 1, "Press Enter to start the game...");
    wrefresh(prompt_win);       // Refresh the window to display changes
    
    // Wait for the player to press Enter
    int ch;
    do {
        ch = wgetch(prompt_win);
    } while (ch != '\n');

    // Clear the prompt window after Enter is pressed
    wclear(prompt_win);
    box(prompt_win, 0, 0);  // Redraw the border for further prompts
    wrefresh(prompt_win);
}


/**
 * Function that checks to see if at any point the player enters either 'quit' or 'exit' at any point during input prompts,
 *      the game will exit normally.
 * 
 * @param prompt_win    The curses window for displaying prompts
 * @param input         The user input being read.
 * @param leave_player  The player that is trying to exit
 * @param oppo_player   The player thats just chillin
 * @param socket_fd     Socket sending the quit message
 */
void player_leave(WINDOW* prompt_win, char* input, const char* leave_player, const char* oppo_player, int socket_fd) {
    // Display the prompt
    mvwprintw(prompt_win, 1, 1, "Enter input (or type 'quit'/'exit' to leave): ");
    wrefresh(prompt_win);

    // Read input from the user in the prompt window
    wgetnstr(prompt_win, input, 256); // 256 is just the buffer size

    // Trim newline char if present
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }

    // Check if the input is 'quit' or 'exit'
    if (strcasecmp(input, "exit") == 0 || strcasecmp(input, "quit") == 0) {
        // Ask the user for confirmation
        wclear(prompt_win);
        mvwprintw(prompt_win, 1, 1, "Only losers rage quit. Are you sure you want to leave the game? (Y/N): ");
        wrefresh(prompt_win);

        char confirm[256];
        wgetnstr(prompt_win, confirm, 256);

        // Check confirmation response
        if (strcasecmp(confirm, "Y") == 0) {
            // If confirmed, notify both players and exit
            char quit_message[256];
            snprintf(quit_message, sizeof(quit_message), "%s rage quit. You win!", leave_player);
            send_message(socket_fd, quit_message);      // Notify the opposing player

            wclear(prompt_win);
            mvwprintw(prompt_win, 1, 1, "Oh well...%s Wins!", oppo_player);
            wrefresh(prompt_win);
            sleep(2);       // Pause before exiting
            end_curses();   // End the curses environment
            exit(0);        // Exit the program
        } else if (strcasecmp(confirm, "N") == 0) {
            // If not confirmed, clear the prompt and return to the last state of the game
            wclear(prompt_win);
            mvwprintw(prompt_win, 1, 1, "Returning to the game...");
            wrefresh(prompt_win);
            sleep(1);   // Pause for clarity
            wclear(prompt_win);
            wrefresh(prompt_win);
            return;        
        } else {
            // Handle input during confirmation
            wclear(prompt_win);
            mvwprintw(prompt_win, 1, 1, "Invalid response. Returning to the game...");
            wrefresh(prompt_win);
            sleep(1);   // Pause for clarity
            wclear(prompt_win);
            wrefresh(prompt_win);
            return;
        }
    } 

    // Clear the prompt window after receiving valid input
    wclear(prompt_win);
    wrefresh(prompt_win);
}
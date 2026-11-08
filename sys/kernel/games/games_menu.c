#include "nwos_internal.h"

void games_menu(void)
{
    char choice[8];

    while (1)
    {
        clear();

        print("\n\n");
        set_color(TERM_COLOR_CYAN);
        menu_border();
        menu_line("            NWOS ARCADE");
        menu_border();
        set_color(base_color);
        print("\n");

        menu_border();
        menu_line("           GAMES MENU");
        menu_border();
        menu_line("  [1]  Guess Number");
        menu_line("  [2]  Guess Word");
        menu_line("  [3]  Rock Paper Scissors");
        menu_line("  [4]  Coin Flip");
        menu_line("  [5]  Dice Roll");
        menu_line("  [6]  Higher / Lower");
        menu_line("  [7]  Math Quiz");
        menu_line("  [8]  Hangman");
        menu_line("  [9]  Tic-Tac-Toe");
        menu_line("  [10] Memory Match");
        menu_line("  [11] Slot Machine");
        menu_line("  [12] Snake");
        menu_line("  [13] Guess Hard");
        menu_border();
        menu_line("  [0]  Back to Shell");
        menu_border();
        print("\n");
        print("                      |\n");
        print("                      -----> ");

        read_line(choice, 8);

        if (strcmp(choice, "0") == 0)
        {
            clear();
            return;
        }
        else if (strcmp(choice, "1") == 0)
        {
            clear();
            game_guess();
        }
        else if (strcmp(choice, "2") == 0)
        {
            clear();
            game_word();
        }
        else if (strcmp(choice, "3") == 0)
        {
            clear();
            game_rps();
        }
        else if (strcmp(choice, "4") == 0)
        {
            clear();
            game_coin();
        }
        else if (strcmp(choice, "5") == 0)
        {
            clear();
            game_dice();
        }
        else if (strcmp(choice, "6") == 0)
        {
            clear();
            game_higher_lower();
        }
        else if (strcmp(choice, "7") == 0)
        {
            clear();
            game_math();
        }
        else if (strcmp(choice, "8") == 0)
        {
            clear();
            game_hangman();
        }
        else if (strcmp(choice, "9") == 0)
        {
            clear();
            game_tictactoe();
        }
        else if (strcmp(choice, "10") == 0)
        {
            clear();
            game_memory();
        }
        else if (strcmp(choice, "11") == 0)
        {
            clear();
            game_slots();
        }
        else if (strcmp(choice, "12") == 0)
        {
            clear();
            game_snake();
        }
        else if (strcmp(choice, "13") == 0)
        {
            clear();
            game_guessh();
        }
        else
        {
            print_error("\nInvalid choice.\n");
            sleep(1000);
        }
    }
}

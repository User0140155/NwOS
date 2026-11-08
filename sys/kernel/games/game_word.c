#include "nwos_internal.h"

void games(void)
{
    print("\nGames:\n");

    print("  guess - Guess the Number\n");
    print("  guessh - Guess number (hard)\n");
    print("  rps   - Rock Paper Scissors\n");
    print("  word - Guess word\n");
    print("  hangman - Hangman\n");
    print("  tictactoe - Tic-Tac-Toe\n");
    print("  game coin - flip a coin\n");
    print("  game dice - roll a dice\n");
    print("  game highlow - guess higher or lower\n");
    print("  game math - game math\n");

    print("\n");
}

void testH(void)
{
    print_success("Hello!\n");
}

void game_word(void)
{
    char answer[32];

    print_title("\n=== GUESS THE WORD ===\n");
    print("Hint: programming language.\n");
    print("You have 3 attempts.\n\n");

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        print("Answer: ");
        read_line(answer, 32);

        if (strcmp(answer, "Rust") == 0)
        {
            print_success("Correct! You win!\n");
            wait_key();
            return;
        }

        print_error("Wrong answer!\n");
    }

    print_error("The answer was: Rust\n");
    wait_key();
}

void generate_num(void) {
    while (1) {
        unsigned int random_num = random_range(1, 100);
        print("Generated random number: ");
        set_color(TERM_COLOR_GREEN);
        print_int(random_num);
        set_color(base_color);
        print("\n");
        char ag[5];
        print("Do you want again? y/n\n");
        read_line(ag, 5);
        if (strcmp(ag, "y") == 0) { }
        else if (strcmp(ag, "n") == 0) {
            break;
        }
        else {
            print_error("Invalid command! automaticaly 'y'\n");
        }
    }
}

void menu_border(void)
{
    print("                    +");
    for (int i = 0; i < MENU_INNER; i++) putchar_os('-');
    print("+\n");
}

void menu_line(const char* text)
{
    print("                    |");
    int n = 0;
    while (text[n]) { putchar_os(text[n]); n++; }
    while (n < MENU_INNER) { putchar_os(' '); n++; }
    print("|\n");
}

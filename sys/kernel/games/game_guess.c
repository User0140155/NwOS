#include "nwos_internal.h"

void game_guess(void)
{
    char buffer[32];

    int secret = random_range(1, 100);

    print("\n=== GUESS THE NUMBER ===\n");
    print("I chose a number from 1 to 100.\n");
    print_error("Type quit to exit.\n\n");

    while (1)
    {
        print("your guess: ");

        read_line(buffer, 32);

        if (strcmp(buffer, "quit") == 0)
        {
            print("You left the game.\n");
            wait_key();
            return;
        }

        int number = atoi_simple(buffer);

        if (number == secret)
        {
            print_success("Correct! You win!\n");
            wait_key();
            return;
        }

        if (number < secret)
        {
            print("Too small!\n");
        }
        else {
            print("Too big!\n");
        }
    }
}

void game_guessh(void)
{
    char bufferh[32];

    int secreth = random_range(1, 250);

    print("\n=== GUESS THE NUMBER HARD===\n");
    print("I chose a number from 1 to 250.\n");
    print_error("Type quit to exit.\n\n");

    while (1)
    {
        print("your guess: ");

        read_line(bufferh, 32);

        if (strcmp(bufferh, "quit") == 0)
        {
            print("You left the game.\n");
            wait_key();
            return;
        }

        int number = atoi_simple(bufferh);

        if (number == secreth)
        {
            print_success("Correct! You win!\n");
            wait_key();
            return;
        }

        if (number < secreth)
            print("Too small!\n");
        else
            print("Too big!\n");
    }
}

void game_dice(void)
{
    int dice;

    print_title("\n=== DICE ===\n\n");

    dice = (rand_simple() % 6) + 1;

    print_success("You rolled: ");
    print_int(dice);
    putchar_os('\n');

    putchar_os('\n');
    wait_key();
}

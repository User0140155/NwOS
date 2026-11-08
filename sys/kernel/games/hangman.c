#include "nwos_internal.h"

char to_upper_letter(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (char)(c - 'a' + 'A');
    }

    return c;
}

void game_hangman(void)
{
    static const char* words[] =
    {
        "KERNEL", "MEMORY", "POINTER", "COMPILE",
        "BINARY", "ARRAY", "LINKER", "BOOTLOADER"
    };

    const int word_count = 8;

    const char* word = words[random_range(0, word_count - 1)];

    int length = 0;

    while (word[length] != '\0')
    {
        length++;
    }

    char revealed[32];

    for (int i = 0; i < length; i++)
    {
        revealed[i] = '_';
    }

    revealed[length] = '\0';

    char guessed[26];

    for (int i = 0; i < 26; i++)
    {
        guessed[i] = 0;
    }

    int lives = 6;
    int found = 0;

    print_title("\n=== HANGMAN ===\n");
    print("Guess the word, one letter at a time.\n");
    print_error("Type quit to exit.\n\n");

    while (lives > 0 && found < length)
    {
        print("Word: ");
        print(revealed);
        print("\nLives: ");
        print_int(lives);
        print("\n\nGuess a letter: ");

        char input[8];

        read_line(input, 8);

        if (strcmp(input, "quit") == 0)
        {
            print("You left the game. The word was: ");
            print(word);
            putchar_os('\n');
            wait_key();
            return;
        }

        char c = to_upper_letter(input[0]);

        if (c < 'A' || c > 'Z')
        {
            print_error("Please enter a single letter.\n\n");
            continue;
        }

        int idx = c - 'A';

        if (guessed[idx])
        {
            print_error("Already guessed that letter.\n\n");
            continue;
        }

        guessed[idx] = 1;

        int hit = 0;

        for (int i = 0; i < length; i++)
        {
            if (word[i] == c)
            {
                revealed[i] = c;
                hit = 1;
                found++;
            }
        }

        if (hit)
        {
            print_success("Good guess!\n\n");
        }
        else
        {
            lives--;
            print_error("Wrong!\n\n");
        }
    }

    if (found == length)
    {
        print_success("You win! The word was: ");
        print(word);
        putchar_os('\n');
    }
    else
    {
        print_error("You lose! The word was: ");
        print(word);
        putchar_os('\n');
    }
    wait_key();
}

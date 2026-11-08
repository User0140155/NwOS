#include "nwos_internal.h"

void ttt_print_board(char board[9])
{
    print("\n");

    for (int row = 0; row < 3; row++)
    {
        print(" ");
        putchar_os(board[row * 3]);
        print(" | ");
        putchar_os(board[row * 3 + 1]);
        print(" | ");
        putchar_os(board[row * 3 + 2]);
        print("\n");

        if (row < 2)
        {
            print("---+---+---\n");
        }
    }

    print("\n");
}

int ttt_check_win(char board[9], char player)
{
    int wins[8][3] =
    {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
        {0, 4, 8}, {2, 4, 6}
    };

    for (int i = 0; i < 8; i++)
    {
        if (board[wins[i][0]] == player &&
            board[wins[i][1]] == player &&
            board[wins[i][2]] == player)
        {
            return 1;
        }
    }

    return 0;
}

int ttt_board_full(char board[9])
{
    for (int i = 0; i < 9; i++)
    {
        if (board[i] != 'X' && board[i] != 'O')
        {
            return 0;
        }
    }

    return 1;
}

void game_tictactoe(void)
{
    char board[9];

    for (int i = 0; i < 9; i++)
    {
        board[i] = (char)('1' + i);
    }

    print_title("\n=== TIC-TAC-TOE ===\n");
    print("You are X, computer is O.\n");
    print("Enter a number 1-9 to place your mark.\n");

    while (1)
    {
        ttt_print_board(board);

        char input[8];

        print("Your move: ");
        read_line(input, 8);

        int move = atoi_simple(input);

        if (move < 1 || move > 9 ||
            board[move - 1] == 'X' || board[move - 1] == 'O')
        {
            print_error("Invalid move.\n");
            continue;
        }

        board[move - 1] = 'X';

        if (ttt_check_win(board, 'X'))
        {
            ttt_print_board(board);
            print_success("You win!\n\n");
            wait_key();
            return;
        }

        if (ttt_board_full(board))
        {
            ttt_print_board(board);
            print("It's a draw!\n\n");
            wait_key();
            return;
        }

        int move_o;

        do
        {
            move_o = random_range(0, 8);
        }
        while (board[move_o] == 'X' || board[move_o] == 'O');

        board[move_o] = 'O';

        if (ttt_check_win(board, 'O'))
        {
            ttt_print_board(board);
            print_error("Computer wins!\n\n");
            wait_key();
            return;
        }

        if (ttt_board_full(board))
        {
            ttt_print_board(board);
            print("It's a draw!\n\n");
            wait_key();
            return;
        }
    }
}

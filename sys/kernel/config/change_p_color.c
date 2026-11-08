#include "nwos_internal.h"

int shell_p = 0;
char invite_user[28] = OS_NAME "> ";

char cfg_user[32] = "user";
char cfg_os[32] = OS_NAME;
char cfg_hostname[64] = "nwos";
char cfg_root_salt[17] = "";
char cfg_root_hash[65] = "";
int cfg_setup_complete = 0;
int cfg_require_login = 0;
int cfg_disk_history = 0;

void change_p_color(void)
{
    int choose_c;

    print("Prompt color:\n");

    set_color(TERM_COLOR_CYAN);           print("1. cyan (default)\n");
    set_color(TERM_COLOR_RED);            print("2. red\n");
    set_color(TERM_COLOR_GREEN);          print("3. green\n");
    set_color(TERM_COLOR_BLUE);           print("4. blue\n");
    set_color(TERM_COLOR_MAGENTA);        print("5. magenta\n");
    set_color(TERM_COLOR_BROWN);          print("6. brown\n");
    set_color(TERM_COLOR_LIGHT_GRAY);     print("7. light gray\n");
    set_color(TERM_COLOR_DARK_GRAY);      print("8. dark gray\n");
    set_color(TERM_COLOR_LIGHT_BLUE);     print("9. light blue\n");
    set_color(TERM_COLOR_LIGHT_GREEN);    print("10. light green\n");
    set_color(TERM_COLOR_LIGHT_CYAN);     print("11. light cyan\n");
    set_color(TERM_COLOR_LIGHT_RED);      print("12. light red\n");
    set_color(TERM_COLOR_LIGHT_MAGENTA);  print("13. light magenta\n");
    set_color(TERM_COLOR_YELLOW);         print("14. yellow\n");
    set_color(TERM_COLOR_WHITE);          print("15. white\n");
    set_color(TERM_COLOR_BLACK);          print("16. black\n");

    set_color(base_color);
    print("Color: ");
    choose_c = read_int();

    if (choose_c < 1 || choose_c > 16)
    {
        print_error("Invalid choose!\n");
        return;
    }

    prompt_color = (unsigned char)(choose_c - 1);
    if ((prompt_color & 0x0F) == TERM_COLOR_BLACK)
        prompt_color = TERM_COLOR_CYAN;

    settings_save();

    print_success("Prompt color set\n");
    print("Preview: ");
    set_color(prompt_color);
    print_shell_p();
    set_color(base_color);
    putchar_os('\n');
}

void t_colorgreen(void)
{
    set_base_color(TERM_COLOR_GREEN);
    settings_save();
    print("setted color to green\n");
}
void t_colorred(void)
{
    set_base_color(TERM_COLOR_RED);
    settings_save();
    print("setted color to red\n");
}
void t_colorgray(void)
{
    set_base_color(TERM_COLOR_LIGHT_GRAY);
    settings_save();
    print("setted color to gray\n");
}
void t_colorblue(void)
{
    set_base_color(TERM_COLOR_BLUE);
    settings_save();
    print("setted color to blue\n");
}
void t_colorcyan(void)
{
    set_base_color(TERM_COLOR_CYAN);
    settings_save();
    print("setted color to cyan\n");
}

void t_colorsetter(void)
{
    int chooseT;
    print("1.green\n");
    print("2.red\n");
    print("3.gray (Default)\n");
    print("4.blue\n");
    print("5.cyan\n");
    chooseT = read_int();
    if (chooseT == 1)
    {
        t_colorgreen();
    }
    if (chooseT == 2)
    {
        t_colorred();
    }
    if (chooseT == 3)
    {
        t_colorgray();
    }
    if (chooseT == 4)
    {
        t_colorblue();
    }
    if (chooseT == 5)
    {
        t_colorcyan();
    }
}

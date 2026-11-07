#include "wordgenerator.h"

#include <stddef.h>
#include <stdint.h>

#include "random/passage_module.h"

void print(const char* text);
void read_line(char* buffer, int max);
void putchar_os(char c);
int random_range(int min, int max);
int read_int(void);
void sleep(unsigned int ms);
const char* nwf_random_word(void);
uint32_t nw_god_random_bounded(uint32_t upper_bound);

static const char* const nw_speak_phrases[] = {
    "I'll think about it",
    "I have an idea",
    "Are you sure?",
    "Oh, really?",
    "What's up?",
    "That sounds interesting",
    "Maybe tomorrow",
    "I give up",
    "That's my favorite",
    "I wonder why",
    "Let's try again",
    "I know what you mean",
    "It could happen",
    "Not so fast",
    "What do you want?",
    "I wasn't expecting that",
    "That is possible",
    "I will remember that",
    "Everything is connected",
    "What happens next?",
    "I need more time",
    "That makes sense",
    "It is a mystery",
    "Here we go again",
    "I found something",
    "Don't worry about it",
    "That was unexpected",
    "I have no idea",
    "Perhaps you are right",
    "Let's see what happens",
    "I can try",
    "This is rather strange",
    "A curious thought",
    "The answer is somewhere",
    "Not today",
    "Maybe it is simple",
    "I like that idea",
    "Why not?",
    "That is enough for now",
    "Something wonderful",
    "I was just thinking",
    "It reminds me of something",
    "What a coincidence",
    "I will ask later",
    "That is the question",
    "Could be better",
    "Could be worse",
    "I understand",
    "I do not understand",
    "The world is complicated",
    "A little patience helps",
    "Try another way",
    "The machine is awake",
    "The machine is dreaming",
    "I am still learning",
    "I have seen this before",
    "That is quite unusual",
    "Nothing is certain",
    "It depends",
    "Let's keep going",
    "A new beginning",
    "An old question",
    "A small discovery",
    "The future is unwritten",
    "I will be here",
    "See you later",
    "Good question",
    "I was not ready",
    "Now I remember",
    "That is funny",
    "It happens sometimes",
    "One thing at a time",
    "We can find out",
    "I hope so",
    "I suppose so",
    "Never say never",
    "The details matter",
    "A strange little world",
    "What a day",
    "Keep that in mind",
    "I have a suggestion",
    "Let's make something",
    "That sounds familiar",
    "There is another way",
    "I will think of something",
    "The question remains",
    "It is worth a try",
    "I like computers",
    "Welcome back",
    "Good to see you",
    "Do you remember?",
    "That is all for now",
    "Maybe someday",
    "A moment of silence",
    "The stars are distant",
    "A quiet afternoon",
    "The signal is clear",
    "Something is changing",
    "I am listening",
    "Tell me more",
    "That is interesting",
    "I had a different idea",
    "We will see",
    "It is a good day",
    "The next step is yours",
    "I think therefore I am",
    "Curiosity is useful",
    "There is no hurry",
    "Let us begin",
    "That was a close one",
    "I can explain",
    "I cannot explain that",
    "The system is ready",
    "Everything looks normal",
    "Something went wrong",
    "Please try again",
    "An interesting result",
    "A different perspective",
    "What is your theory?",
    "I will leave that to you",
    "That is a good point",
    "It may be important",
    "The answer may surprise you",
    "No further questions",
    "I have a feeling",
    "A little bit of luck",
    "The show must go on"
};

#define NW_SPEAK_PHRASE_COUNT \
    ((unsigned int)(sizeof(nw_speak_phrases) / sizeof(nw_speak_phrases[0])))

#define NW_SPEAK_MIN_WORDS 5u
#define NW_SPEAK_MAX_WORDS 16u
#define NW_BIBLE_PASSAGE_VERSES 8u

static int nw_word_is_exit_command(const char* text)
{
    static const char command[] = "!0";
    int i = 0;

    if (text == 0)
        return 0;

    while (command[i] != '\0')
    {
        if (text[i] != command[i])
            return 0;
        ++i;
    }

    return text[i] == '\0';
}

static char nw_ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return (char)(c - 'A' + 'a');
    return c;
}

static int nw_is_ascii_letter(char c)
{
    c = nw_ascii_lower(c);
    return c >= 'a' && c <= 'z';
}

static int nw_text_has_word(const char* text, const char* needle)
{
    int i;

    if (text == 0 || needle == 0 || needle[0] == '\0')
        return 0;

    for (i = 0; text[i] != '\0'; ++i)
    {
        int j = 0;
        while (needle[j] != '\0' && text[i + j] != '\0' &&
               nw_ascii_lower(text[i + j]) == nw_ascii_lower(needle[j]))
            ++j;

        if (needle[j] == '\0' &&
            (i == 0 || !nw_is_ascii_letter(text[i - 1])) &&
            !nw_is_ascii_letter(text[i + j]))
            return 1;
    }

    return 0;
}

static unsigned int nw_phrase_word_count(const char* phrase)
{
    unsigned int count = 0;
    int inside_word = 0;

    while (*phrase != '\0')
    {
        char c = *phrase++;
        int is_space = (c == ' ' || c == '\t' || c == '\r' || c == '\n');

        if (is_space)
            inside_word = 0;
        else if (!inside_word)
        {
            inside_word = 1;
            ++count;
        }
    }

    return count;
}

static void nw_speak_emit(void)
{
    unsigned char used[NW_SPEAK_PHRASE_COUNT];
    unsigned int target_words;
    unsigned int output_words = 0;
    unsigned int attempts = 0;
    unsigned int printed = 0;

    for (unsigned int i = 0; i < NW_SPEAK_PHRASE_COUNT; ++i)
        used[i] = 0;

    target_words = NW_SPEAK_MIN_WORDS +
        (unsigned int)nw_god_random_bounded(
            NW_SPEAK_MAX_WORDS - NW_SPEAK_MIN_WORDS + 1u);

    /* Choose complete, unique phrases until the answer contains 5..16 words.
       Unlike the earlier 32-line collage, this keeps chat replies compact. */
    while (output_words < target_words &&
           attempts < NW_SPEAK_PHRASE_COUNT * 8u)
    {
        unsigned int index = (unsigned int)nw_god_random_bounded(
            NW_SPEAK_PHRASE_COUNT);
        unsigned int phrase_words;

        ++attempts;
        if (used[index])
            continue;

        used[index] = 1;
        phrase_words = nw_phrase_word_count(nw_speak_phrases[index]);

        if (phrase_words == 0u || phrase_words > target_words - output_words)
            continue;

        if (printed != 0u)
            putchar_os(' ');
        print(nw_speak_phrases[index]);
        output_words += phrase_words;
        ++printed;
    }

    /* The phrase bank has enough short entries to reach every target exactly.
       Keep a visible fallback if that invariant is broken by a future edit. */
    if (output_words < NW_SPEAK_MIN_WORDS)
        print("I have something to say");
}

void nw_god_bible_passage(void)
{
    uint32_t total;
    uint32_t line_count;
    uint32_t start;

    if (!nw_passage_module_is_ready())
    {
        print("\n[NwPassage] Bible text module is not loaded.\n");
        print("Place the unchanged WEB verse-per-line text at:\n");
        print("  data/passages/world_english_bible.txt\n");
        return;
    }

    total = nw_passage_module_count();
    if (total == 0u)
    {
        print("\n[NwPassage] No usable verses were found.\n");
        return;
    }

    line_count = (total < NW_BIBLE_PASSAGE_VERSES) ? total : NW_BIBLE_PASSAGE_VERSES;
    start = (uint32_t)nw_god_random_bounded(total - line_count + 1u);

    print("\n=== NwOS RANDOM PASSAGE (WORLD ENGLISH BIBLE) ===\n\n");
    for (uint32_t i = 0; i < line_count; ++i)
    {
        const char *line = 0;
        uint32_t length = 0;

        if (!nw_passage_module_get(start + i, &line, &length))
            break;

        for (uint32_t j = 0; j < length; ++j)
            putchar_os(line[j]);
        putchar_os('\n');
    }
    putchar_os('\n');
}

static void nw_print_random_words(int count, int delay_words)
{
    int i;

    for (i = 0; i < count; ++i)
    {
        const char* word = nwf_random_word();

        if (word == 0 || word[0] == '\0')
            continue;

        if (i != 0)
            putchar_os(' ');
        print(word);

        if (delay_words)
            sleep((unsigned int)random_range(195, 785));
    }
}

void say(void)
{
    print("\n\nNwOS says: ");
    nw_speak_emit();
    print(".\n\n");
}

void word_new(void)
{
    int words_count;

    print("Words (1-500): ");
    words_count = read_int();

    if (words_count < 1 || words_count > 500)
    {
        print("Please enter a number from 1 to 500.\n");
        return;
    }

    nw_print_random_words(words_count, 1);
    print("\n");
}

void chat(void)
{
    char input[128];

    print("\n================================\n");
    print("       NWOS GODSPEAK CHAT\n");
    print("================================\n");
    print("Type !0 to leave the chat.\n");
    print("This is a random phrase oracle, not a language model.\n\n");

    while (1)
    {
        print("You: ");
        read_line(input, sizeof(input));

        if (nw_word_is_exit_command(input))
        {
            print("\nNwOS: Goodbye!\n");
            print("Chat closed.\n\n");
            return;
        }

        print("\nNwOS: ");

        /* Tiny deterministic responses for the common greeting/name example.
           All other input uses the TempleOS-style random phrase collage. */
        if (nw_text_has_word(input, "name"))
        {
            print("I am NwOS, your operating system.");
        }
        else if (nw_text_has_word(input, "how are you"))
        {
            print("I am running normally. Thanks for asking.");
        }
        else if (nw_text_has_word(input, "hello") ||
                 nw_text_has_word(input, "hi") ||
                 nw_text_has_word(input, "hey"))
        {
            print("Hello! Good to see you.");
        }
        else
        {
            nw_speak_emit();
            putchar_os('.');
        }

        print("\n\n");
    }
}

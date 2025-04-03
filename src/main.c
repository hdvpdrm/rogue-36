#include <curses.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mach_dep.h"
#include "rogue.h"

#ifdef CHECKTIME
static int num_checks = 0;  /* Times we've gone over in checkout() */
#endif

int main(int argc, char **argv, char **envp)
{
    char *env;
    struct passwd *pw;
    struct linked_list *item;
    struct object *obj;
    int lowtime;
    long now;

    /* Check for print-score option */
    if (argc == 2 && strcmp(argv[1], "-s") == 0)
      {
        waswizard = 1;
        score(0, -1);
        exit(0);
      }

    /* Check to see if he is a wizard */
    if (argc >= 2 && argv[1][0] == '\0')
      {
        if (strcmp(PASSWD, getpass("Wizard's password: ")) == 0)
	  {
            wizard = 1;
            argv++;
            argc--;
        }
    }

    /* Get home and options from environment */
    if ((env = getenv("HOME")) != NULL)
      {
        strcpy(home, env);
    }
    else if ((pw = getpwuid(getuid())) != NULL)
      {
        strcpy(home, pw->pw_dir);
    }
    else
      {
        home[0] = '\0';
      }
    strcat(home, "/");

    strcpy(file_name, home);
    strcat(file_name, "rogue.save");

    if ((env = getenv("ROGUEOPTS")) != NULL)
      {
        parse_opts(env);
      }
    if (env == NULL || whoami[0] == '\0')
      {
        if ((pw = getpwuid(getuid())) == NULL) {
	  printf("Say, who the hell are you?\n");
	  exit(1);
        }
	else
	  {
            strncpy(whoami, pw->pw_name, strlen(pw->pw_name));
	  }
      }
    if (env == NULL || fruit[0] == '\0')
      {
	strcpy(fruit, "slime-mold");
      }

#if MAXLOAD || MAXUSERS
    if (too_much() && !wizard && !author())
      {
        printf("Sorry, %s, but the system is too loaded now.\n", whoami);
        printf("Try again later.  Meanwhile, why not enjoy a%s %s?\n",
               vowelstr(fruit), fruit);
        exit(1);
      }
#endif
    if (argc == 2)
      {
        if (!restore(argv[1], envp)) /* Note: restore will never return */
	  exit(1);
      }
    time(&now);
    lowtime = (int)now;
    dnum = (wizard && getenv("SEED") != NULL ?
            atoi(getenv("SEED")) :
            lowtime + getpid());
    if (wizard)
      {
        printf("Hello %s, welcome to dungeon #%d", whoami, dnum);
      }
    else
      {
        printf("Hello %s, just a moment while I dig the dungeon...", whoami);
      }
    fflush(stdout);
    seed = dnum;

    init_player();          /* Roll up the rogue */
    init_things();          /* Set up probabilities of things */
    init_names();           /* Set up names of scrolls */
    init_colors();          /* Set up colors of potions */
    init_stones();          /* Set up stone settings of rings */
    init_materials();       /* Set up materials of wands */
    initscr();              /* Start up cursor package */
    setup();

    /* Set up windows */
    cw = newwin(LINES, COLS, 0, 0);
    mw = newwin(LINES, COLS, 0, 0);
    hw = newwin(LINES, COLS, 0, 0);
    waswizard = wizard;
    new_level();            /* Draw current level */

    /* Start up daemons and fuses */
    daemon(doctor, 0, AFTER);
    fuse(swander, 0, WANDERTIME, AFTER);
    daemon(stomach, 0, AFTER);
    daemon(runners, 0, AFTER);

    /* Give the rogue his weaponry.  First a mace. */
    item = new_item(sizeof *obj);
    obj = (struct object *)ldata(item);
    obj->o_type = WEAPON;
    obj->o_which = MACE;
    init_weapon(obj, MACE);
    obj->o_hplus = 1;
    obj->o_dplus = 1;
    obj->o_flags |= ISKNOW;
    add_pack(item, 1);
    cur_weapon = obj;

    /* Now a +1 bow */
    item = new_item(sizeof *obj);
    obj = (struct object *)ldata(item);
    obj->o_type = WEAPON;
    obj->o_which = BOW;
    init_weapon(obj, BOW);
    obj->o_hplus = 1;
    obj->o_dplus = 0;
    obj->o_flags |= ISKNOW;
    add_pack(item, 1);

    /* Now some arrows */
    item = new_item(sizeof *obj);
    obj = (struct object *)ldata(item);
    obj->o_type = WEAPON;
    obj->o_which = ARROW;
    init_weapon(obj, ARROW);
    obj->o_count = 25 + rnd(15);
    obj->o_hplus = obj->o_dplus = 0;
    obj->o_flags |= ISKNOW;
    add_pack(item, 1);

    /* And his suit of armor */
    item = new_item(sizeof *obj);
    obj = (struct object *)ldata(item);
    obj->o_type = ARMOR;
    obj->o_which = RING_MAIL;
    obj->o_ac = a_class[RING_MAIL] - 1;
    obj->o_flags |= ISKNOW;
    cur_armor = obj;
    add_pack(item, 1);

    /* Give him some food too */
    item = new_item(sizeof *obj);
    obj = (struct object *)ldata(item);
    obj->o_type = FOOD;
    obj->o_count = 1;
    obj->o_which = 0;
    add_pack(item, 1);

    playit();
    return 0;
}

/* Endit: Exit the program abnormally. */
void endit(int signum)
{
    fatal("Ok, if you want to exit that badly, I'll have to allow it\n");
}

/* Fatal: Exit the program, printing a message. */
void fatal(const char *s)
{
    clear();
    move(LINES - 2, 0);
    printw("%s", s);
    refresh();
    endwin();
    exit(0);
}

/* Rnd: Pick a very random number. */
int rnd(int range)
{
    return range == 0 ? 0 : abs(RN) % range;
}

/* Roll: Roll a number of dice. */
int roll(int number, int sides)
{
    int dtotal = 0;
    while (number--)
      {
        dtotal += rnd(sides) + 1;
      }
    return dtotal;
}

#ifdef SIGTSTP
/* Handle stop and start signals. */
void tstp(int signum)
{
    mvcur(0, COLS - 1, LINES - 1, 0);
    endwin();
    fflush(stdout);
    kill(0, SIGTSTP);
    signal(SIGTSTP, tstp);
    crmode();
    noecho();
    clearok(curscr, 1);
    touchwin(cw);
    wrefresh(cw);
}
#endif

void setup()
{
#ifdef CHECKTIME
    void checkout();
#endif
    void segv_handler(int);

#ifndef DUMP
    signal(SIGHUP, auto_save);
    signal(SIGILL, auto_save);
    signal(SIGTRAP, auto_save);
    signal(SIGIOT, auto_save);
#ifdef SIGEMT
    signal(SIGEMT, auto_save);
#endif
    signal(SIGFPE, auto_save);
    signal(SIGBUS, auto_save);
    signal(SIGSEGV, segv_handler);
    signal(SIGSYS, auto_save);
    signal(SIGPIPE, auto_save);
    signal(SIGTERM, auto_save);
#endif

    signal(SIGINT, quitgame);
#ifndef DUMP
    signal(SIGQUIT, endit);
#endif
#ifdef SIGTSTP
    signal(SIGTSTP, tstp);
#endif
#ifdef CHECKTIME
    if (!author())
      {
        signal(SIGALRM, checkout);
        alarm(CHECKTIME * 60);
        num_checks = 0;
      }
#endif
    crmode();                   /* Cbreak mode */
    noecho();                   /* Echo off */
}

/* Playit: The main loop of the program. Loop until the game is over, refreshing things and looking at the proper times. */
void playit()
{
    char *opts;

    /* Set up defaults for slow terminals */
    if (0) /* detect slow terminal, disabled */
      {
        terse = 1;
        jump = 1;
      }

    /* Parse environment declaration of options */
    if ((opts = getenv("ROGUEOPTS")) != NULL)
      {
        parse_opts(opts);
      }

    oldpos = hero;
    oldrp = roomin(&hero);
    while (playing)
      {
        command();                 /* Command execution */
      }
    endit(1);
}

#if MAXLOAD || MAXUSERS
/* See if the system is being used too much for this game. */
int too_much()
{
#ifdef MAXLOAD
    double avec[3];
#else
    int cnt;
#endif

#ifdef MAXLOAD
    loadav(avec);
    return (avec[2] > (MAXLOAD / 10.0));
#else
    return (ucount() > MAXUSERS);
#endif
}

/* See if a user is an author of the program. */
int author()
{
    switch (getuid())
      {
        case 24601:
            return 1;
        default:
            return 0;
      }
}
#endif

#ifdef CHECKTIME
void checkout()
{
    static const char *msgs[] = {
        "The load is too high to be playing.  Please leave in %d minutes",
        "Please save your game.  You have %d minutes",
        "Last warning.  You have %d minutes to leave",
    };
    int checktime;

    signal(SIGALRM, checkout);
    if (too_much())
      {
        if (num_checks == 3)
	  {
            fatal("Sorry.  You took too long.  You are dead\n");
	  }
        checktime = CHECKTIME / (num_checks + 1);
        chmsg(msgs[num_checks++], checktime);
        alarm(checktime * 60);
      }
    else
      {
        if (num_checks)
	  {
            chmsg("The load has dropped back down.  You have a reprieve.");
            num_checks = 0;
	  }
        alarm(CHECKTIME * 60);
      }
}

/* Checkout()'s version of msg.  If we are in the middle of a shell, do a printf instead of a msg to avoid the refresh. */
void chmsg(const char *fmt, int arg)
{
    if (in_shell)
      {
        printf(fmt, arg);
        putchar('\n');
        fflush(stdout);
      }
    else
      {
        msg(fmt, arg);
      }
}
#endif

#ifdef LOADAV

#include <nlist.h>

struct nlist avenrun = { "_avenrun" };

void loadav(double *avg)
{
    int kmem;

    if ((kmem = open("/dev/kmem", 0)) < 0)
      {
        goto bad;
      }
    nlist(NAMELIST, &avenrun);
    if (avenrun.n_type == 0)
      {
bad:
        avg[0] = avg[1] = avg[2] = 0.0;
        return;
      }

    lseek(kmem, (long)avenrun.n_value, 0);
    read(kmem, avg, 3 * sizeof(double));
}
#endif

#ifdef UCOUNT

#include <utmp.h>

struct utmp buf;

int ucount()
{
    struct utmp *up;
    FILE *utmp;
    int count = 0;

    if ((utmp = fopen(UTMP, "r")) == NULL)
      {
        return 0;
      }

    up = &buf;
    while (fread(up, 1, sizeof(*up), utmp) > 0)
      {
        if (buf.ut_name[0] != '\0')
	  {
            count++;
	  }
      }
    fclose(utmp);
    return count;
}
#endif

void segv_handler(int sig)
{
    void *arr[16];
    int sz = backtrace(arr, 16);
    fdbg("SEGV");
    endwin();
    backtrace_symbols_fd(arr, sz, STDERR_FILENO);
}

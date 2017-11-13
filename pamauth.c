/*
 *
 * TODO:
 * - show issue
 * - show motd?
 * - check nologin (https://github.com/mozilla-b2g/busybox/blob/master/loginutils/login.c#L45)
 * - openlog (https://github.com/mozilla-b2g/busybox/blob/master/loginutils/login.c#L294)
 * - getopt: rhost, wishcmd, pam_service_name
 * - get rhost/ruser
 * - utmp entry?
 * - init selinux (https://github.com/mozilla-b2g/busybox/blob/master/loginutils/login.c#L94)
 * 
 */
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <grp.h>
#include <sys/wait.h>

#define errstr		(strerror(errno))
#define UNUSED(x)	((void)(x))

enum {
  USER_LEN = 64,
  RESP_LEN = 256,
  MAX_TRIES = 3,
  TIMEOUT = 60,
};

static const char *wish_cmd = "/usr/bin/wish";
static const char *pam_servicename = "login";
static const char *default_shell = "/bin/sh";
static const char *default_path = "/bin:/usr/bin";
static int filenos[2];

void die(int retcode,const char *msgfmt, ...) {
  va_list ap;

  puts("exit");
  va_start(ap,msgfmt);
  vfprintf(stderr,msgfmt,ap);
  va_end(ap);
  exit(retcode);
}

void setup_gui(void) {
  int fda[2], fdb[2];
  pid_t cpid;

  if (pipe(fda) == -1) die(__LINE__,"pipe: %s\n", errstr);
  if (pipe(fdb) == -1) die(__LINE__,"pipe: %s\n", errstr);

  cpid = fork();
  switch (cpid) {
    case -1:
      die(__LINE__,"fork: %s\n", errstr);
    case 0:
      dup2(fda[0],0);
      dup2(fdb[1],1);

      close(fda[0]); close(fda[1]);
      close(fdb[0]); close(fdb[1]);
      execlp(wish_cmd, wish_cmd, NULL);
      die(__LINE__,"exec(%s): %s\n", wish_cmd, errstr);
    default:
      filenos[0] = dup(0);
      filenos[1] = dup(1);
      dup2(fdb[0],0);
      dup2(fda[1],1);
      close(fda[0]); close(fda[1]);
      close(fdb[0]); close(fdb[1]);
  }

  setvbuf(stdout,NULL,_IONBF,0);
  puts("wm title . {PAM Auth}");

}

void strtrim(char *str) {
  int l =strlen(str);
  if (l == 0) return;
  --l;
  while (isspace(str[l])) {
    str[l--] = 0;
  }
}

int conv_fun(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdat_ptr) {
  const struct pam_message *msg_ptr = *msg;
  struct pam_response *resp_ptr;
  char buf[RESP_LEN];
  int i;
  UNUSED(appdat_ptr);

  puts("destroy {*}[winfo children .]");
  resp_ptr = *resp = calloc(sizeof(struct pam_response),num_msg);
  for (i = 0;i < num_msg; i++) {
    fprintf(stderr,"%d) %d %s\n",i, msg_ptr[i].msg_style, msg_ptr[i].msg);
    switch (msg_ptr[i].msg_style) {
      case PAM_PROMPT_ECHO_ON:
      case PAM_PROMPT_ECHO_OFF:
	printf("frame .b%d\n",i);
	printf("label .l%d -text {%s}\n", i, msg_ptr[i].msg);
	fprintf(stderr,"label .l%d -text {%s}\n", i, msg_ptr[i].msg);
	if (msg_ptr[i].msg_style == PAM_PROMPT_ECHO_OFF) {
	  printf("entry .e%d -text {} -show {*}\n",i);
	  fprintf(stderr,"entry .e%d -text {} -show {*}\n",i);
	} else {
	  printf("entry .e%d -text {}\n",i);
	  fprintf(stderr,"entry .e%d -text {}\n",i);
	}
	printf("pack .l%d .e%d -side left -in .b%d\n",i,i,i);
	printf("pack .b%d\n",i);
	
	printf("focus -force .e%d\n",i);
	printf("bind . <Key-Return> { puts [.e%d get] ; bind . <Key-Return> {} }\n",i);
	fgets(buf, sizeof(buf)-1, stdin); strtrim(buf);
	resp_ptr[i].resp = strdup(buf);
	puts("destroy {*}[winfo children .]");
	break;
      case PAM_ERROR_MSG:
	printf("label .l%d -text {%s} -fg red\n", i, msg_ptr[i].msg);
	printf("pack .l%d\n",i);
	break;
      case PAM_TEXT_INFO:
	printf("label .l%d -text {%s}\n", i, msg_ptr[i].msg);
	printf("pack .l%d\n",i);
	break;
      default:
	die(__LINE__,"Unknown pam msg: %d, %d\n", msg_ptr[i].msg_style, i);
    }
  }
  return PAM_SUCCESS;
}

static void alarm_handler(int sig)
{
  UNUSED(sig);
  /* This is the escape hatch!  Poor serial line users and the like
   * arrive here when their connection is broken.
   * We don't want to block here
   */
  puts("exit");
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  pam_handle_t *pamh=NULL;
  int retval;
  char user[USER_LEN];
  struct pam_conv conv;
  const char *errmsg = NULL;
  int cnt = MAX_TRIES;
  struct passwd pwdstruct, *pw;
  char pwdbuf[256];
  const char *changed_username = NULL;
  pid_t session;
  char **script;
  int status;
  char **pam_envlist, **pam_env;

  do {
    int n,i;
    n = sysconf(_SC_OPEN_MAX);
    for(i = 3; i < n; i++) close(i);
  } while (0);

  memset(&conv, 0, sizeof conv);
  conv.conv = &conv_fun;
  conv.appdata_ptr = NULL;

  UNUSED(argc);UNUSED(argv);

  /* Set-up GUI */
  setup_gui();
  script = argv+1;
  signal(SIGALRM, alarm_handler);
  alarm(TIMEOUT);

  while (cnt--) {
    puts("destroy {*}[winfo children .]");
    puts("frame .b");
    puts("label .l -text {Login: }");
    puts("entry .e -text {}");
    puts("pack .l .e -side left -in .b");
    puts("pack .b");
    if (errmsg) {
      printf("label .m -text {%s} -fg red\n", errmsg);
      puts("pack .m");
    }
    puts("focus -force .e");
    puts("bind . <Key-Return> { puts [.e get] ; bind . <Key-Return> {} }");

    fgets(user, sizeof(user)-1, stdin); strtrim(user);
    fprintf(stderr, ">> %s\n", user);

    if ((retval = pam_start(pam_servicename, user, &conv, &pamh)) != PAM_SUCCESS) goto pam_failed;
    if ((retval = pam_authenticate(pamh, 0)) != PAM_SUCCESS) goto pam_failed;
    if ((retval = pam_acct_mgmt(pamh, 0)) == PAM_NEW_AUTHTOK_REQD)
      retval = pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
    if (retval != PAM_SUCCESS) goto pam_failed;
    if ((retval = pam_setcred(pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS) goto pam_failed;
    if ((retval = pam_open_session(pamh, 0)) != PAM_SUCCESS) goto pam_failed;

    if ((retval = pam_get_item(pamh, PAM_USER, (const void **)&changed_username)) != PAM_SUCCESS) goto pam_failed;
    if (changed_username != NULL) {
      fprintf(stderr, "PAM: %s\n", changed_username);
      strncpy(user,changed_username,sizeof(user));
      user[sizeof(user)-1] = 0;
    }
    alarm(0);

    /* clearenv but remember certain things... */
    do {
      struct {
	const char *key;
	const char *val;
      } keep_env[] = {
	{ "TERM" , NULL },
	{ "DISPLAY", NULL },
	{ NULL, NULL },
      };
      int i;
      for (i=0; keep_env[i].key ; i++) {
	keep_env[i].val = getenv(keep_env[i].key);
      }
      clearenv();
      for (i=0; keep_env[i].key ; i++) {
	if (keep_env[i].val) setenv(keep_env[i].key, keep_env[i].val, 1);
      }
    } while (0);
    
    /* export PAM environment */
    if ((pam_envlist = pam_getenvlist(pamh)) != NULL) {
      fprintf(stderr,"Exporting environment...\n");
      for (pam_env = pam_envlist; *pam_env != NULL; ++pam_env) {
	putenv(*pam_env);
	fprintf(stderr,"\t%s\n",*pam_env);
	free(*pam_env);
      }
      free(pam_envlist);
    }

    /* Don't use "pw = getpwnam(username);",
     * PAM is said to be capable of destroying static storage
     * used by getpwnam(). We are using safe(r) function */
    pw = NULL;
    getpwnam_r(user, &pwdstruct, pwdbuf, sizeof(pwdbuf), &pw);
    if (!pw) {
      errmsg = "Login failed";
      goto auth_error;
    }
    puts("exit"); fflush(stdout);

    session = fork();
    switch (session) {
      case -1:
	die(__LINE__,"fork: %s\n", errstr);
      case 0:
	/* child: give up privs and start script */

	/* set uid and groups */
	if (initgroups(pw->pw_name, pw->pw_gid) == -1) die(__LINE__,"initgroups: %s\n", errstr);
	if (setgid(pw->pw_gid) == -1) die(__LINE__,"setgid: %s\n", errstr);
	if (setuid(pw->pw_uid) == -1) die(__LINE__,"setuid: %s\n", errstr);

	if (chdir(pw->pw_dir) == -1) {
	  fprintf(stderr,"can't chdir to home directory %s (%s)\n", pw->pw_dir, errstr);
	  chdir("/");
	}
	setenv("USER", pw->pw_name,1);
	setenv("LOGNAME", pw->pw_name,1);
	setenv("HOME", pw->pw_dir,1);
	setenv("SHELL", pw->pw_shell && pw->pw_shell[0] ? pw->pw_shell: default_shell,1);
	setenv("PATH", default_path,1);

	dup2(filenos[0],0); close(filenos[0]);
	dup2(filenos[1],1); close(filenos[1]);
	
	execvp(*script, script);
	die(__LINE__,"exec(%s): %s\n",*script, errstr);
    }
    waitpid(session,&status,0);
    retval = pam_close_session(pamh, 0);
    pam_end(pamh,retval);
    break;
  pam_failed:
    errmsg = pam_strerror(pamh, retval);
  auth_error:
    pam_close_session(pamh, 0);
    pam_end(pamh,retval);
  }
  puts("exit");

}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>

#define MAXLEN 2048

void usage(void)
{
	printf("sshdlogreport: read /var/log/auth.log, report failed log information to url\n\n");
	printf
	    ("sshdlogreport [ -d ] [ -k keyfile ] [ -h ] [ -l logfile ] [ -u report_url ] [ -r reportip ]\n\n");
	printf("   -d             enable debug\n");
	printf("   -k keyfile     keyfile, default is keyfile.txt\n");
	printf("   -l logfile     logfile path, default is /var/log/auth.log or /var/log/secure\n");
	printf
	    ("   -u report_url  report_url, default is http://blackip.ustc.edu.cn/sshdlogreport.php\n");
	printf("   -r reportip    report ip\n\n");
	printf("report method POST:\n");
	printf("   curl -d @- report_url\n");
	printf
	    ("   POST data    apikey=key&reportip=IP&ip=IP&username=USERNAME&count=COUNT&msg=sshd_fail_log\n\n");
	exit(0);
}

char apikey[MAXLEN];
char logfile[MAXLEN];
char report_url[MAXLEN];
char reportip[MAXLEN];
int debug = 0;

void error(char *msg)
{
	perror(msg);
	exit(1);
}

void daemon_init(void)
{
	int pid;
	pid = fork();
	if (pid == -1)
		error("fork error!");
	else if (pid > 0)
		exit(0);

	if (setsid() == -1)
		error("setsid error!");

	pid = fork();
	if (pid == -1)
		error("fork error!");
	else if (pid > 0)
		exit(0);
	setsid();
	close(0);
	close(1);
	close(2);
}

static unsigned char hexchars[] = "0123456789ABCDEF";

int URLEncode(const char *str, const int strsz, char *result, const int resultsz)
{
	int i, j;
	char ch;

	if (strsz < 0 || resultsz < 0)
		return -1;

	for (i = 0, j = 0; i < strsz && j < resultsz; i++) {
		ch = *(str + i);
		if ((ch >= 'A' && ch <= 'Z') ||
		    (ch >= 'a' && ch <= 'z') ||
		    (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '*' || ch == '_')
			result[j++] = ch;
		else if (ch == ' ')
			result[j++] = '+';
		else {
			if (j + 3 <= resultsz - 1) {
				result[j++] = '%';
				result[j++] = hexchars[(unsigned char)ch >> 4];
				result[j++] = hexchars[(unsigned char)ch & 0xF];
			} else {
				return -2;
			}
		}
	}

	if (i == 0) {
		result[0] = 0;
		return 0;
	} else if (i == strsz) {
		result[j] = 0;
		return j;
	}
	result[0] = 0;
	return -2;
}

void readkey(char *fname)
{
	FILE *fp;
	char buf[MAXLEN];
	fp = fopen(fname, "r");
	if (fp == NULL)
		return;
	if (fgets(buf, MAXLEN, fp)) {
		if (strlen(buf) <= 1) {
			fclose(fp);
			return;
		}
		if (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = 0;
		strncpy(apikey, buf, MAXLEN);
	}
	fclose(fp);
}

int fileexists(const char *filename)
{
	FILE *file;
	file = fopen(filename, "r");
	if (file != NULL) {
		fclose(file);
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	FILE *fp;
	char buf[MAXLEN];
	char bufcmd[MAXLEN];
	int c;
	while ((c = getopt(argc, argv, "dhk:l:u:r:")) != EOF)
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'h':
			usage();
		case 'k':
			readkey(optarg);
			break;
		case 'l':
			strncpy(logfile, optarg, MAXLEN);
			break;
		case 'u':
			strncpy(report_url, optarg, MAXLEN);
			break;
		case 'r':
			strncpy(reportip, optarg, MAXLEN);
			break;
		}
	if (apikey[0] == 0)
		readkey("apikey.txt");
	if (apikey[0] == 0) {
		fprintf(stderr, "ERROR: apkey = NULL\n\n");
		usage();
	}
	if (logfile[0] == 0) {
		if (fileexists("/var/log/auth.log"))
			strcpy(logfile, "/var/log/auth.log");
		else
			strcpy(logfile, "/var/log/secure");
	}
	if (report_url[0] == 0)
		strcpy(report_url, "http://blackip.ustc.edu.cn/sshdlogreport.php");

	if (debug) {
		printf("apikey: %s\n", apikey);
		printf("logfile: %s\n", logfile);
		printf("report_url: %s\n", report_url);
		printf("rreportip: %s\n", reportip);
	} else
		daemon_init();

	snprintf(bufcmd, MAXLEN, "tail --retry --follow=name --max-unchanged-stats=5 %s", logfile);

	fp = popen(bufcmd, "r");
	if (fp == NULL) {
		printf("popen error\n");
		exit(0);
	}

/* sample log
Jan  8 06:03:23 ipv6 sshd[6274]: message repeated 3 times: [ Failed password for root from 112.85.42.178 port 62234 ssh2]
Jan  8 06:03:48 ipv6 sshd[7442]: Failed password for root from 112.85.42.178 port 6282 ssh2
Jan  5 13:21:14 ipv6 sshd[22356]: Invalid user git from 210.45.66.85 port 48769

do not process the following line, it's a repeat of line 3
Jan  8 06:04:49 ipv6 sshd[8917]: Failed password for invalid user zvj from 192.83.166.81 port 60726 ssh2
*/
	while (fgets(buf, MAXLEN, fp)) {
		int count = 1;
		char *p, *username, *fromip, *port;
		char msgbuf[MAXLEN * 3];
		if (debug)
			fprintf(stderr, "Got: %s", buf);
		if (strlen(buf) < 10)
			continue;

		if (strstr(buf, "Failed password for invalid user") != NULL)
			continue;
		if ((strstr(buf, "Failed password") == NULL)
		    && (strstr(buf, "Invalid user") == NULL))
			continue;

		if (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = 0;
		URLEncode(buf, strlen(buf), msgbuf, MAXLEN * 3);

		p = strstr(buf, "message repeated ");
		if (p) {
			count = atoi(p);
			if (debug)
				fprintf(stderr, "get count: %d\n", count);
		}

		p = strstr(buf, "Failed password for ");
		if (p == NULL) {
			p = strstr(buf, "Invalid user ");
			if (p == NULL)
				continue;
			p = p + strlen("Invalid user ");
		} else
			p = p + strlen("Failed password for ");

		username = p;
		while (*p && *p != ' ')
			p++;
		if (*p == 0)
			continue;
		*p = 0;
		p++;
		if (memcmp(p, "from ", 5) != 0)
			continue;
		p = p + 5;
		if (*p == 0)
			continue;
		fromip = p;
		while (*p && *p != ' ')
			p++;

		if (*p == 0) {
			port = "";
		} else {
			*p = 0;
			p++;
			if (memcmp(p, "port ", 5) != 0) {
				port = "";
			} else {
				p = p + 5;
				if (*p == 0) {
					port = "";
				} else {

					port = p;
					while (*p && *p != ' ')
						p++;
					*p = 0;
				}
			}
		}
		if (debug)
			fprintf(stderr, "count: %d, username: %s, fromip: %s, port: %s\n",
				count, username, fromip, port);

		snprintf(bufcmd, MAXLEN, "curl -d @- %s", report_url);
		FILE *fpcmd;
		fpcmd = popen(bufcmd, "w");
		if (fpcmd == NULL) {
			if (debug)
				fprintf(stderr, "popen %s error\n", bufcmd);
			continue;
		}
		fprintf(fpcmd,
			"apikey=%s&reportip=%s&ip=%s&count=%d&username=%s&port=%s&msg=%s",
			apikey, reportip, fromip, count, username, port, msgbuf);
		if (debug)
			fprintf(stderr,
				"send to curl: apikey=%s&reportip=%s&ip=%s&count=%d&username=%s&port=%s&msg=%s\n",
				apikey, reportip, fromip, count, username, port, msgbuf);
		pclose(fpcmd);
	}
	pclose(fp);
	return 0;
}


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <libnetconf.h>

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#define CAPABILITY_PREFIX "urn:ietf:params:netconf:"
#define ARGUMENTS "f:hl:p:c:vd"

void clb_print(NC_VERB_LEVEL level, const char* msg)
{
	switch (level) {
	case NC_VERB_ERROR:
		fprintf(stderr, "libnetconf ERROR: %s\n", msg);
		break;
	case NC_VERB_WARNING:
		fprintf(stderr, "libnetconf WARNING: %s\n", msg);
		break;
	case NC_VERB_VERBOSE:
		fprintf(stderr, "libnetconf VERBOSE: %s\n", msg);
		break;
	case NC_VERB_DEBUG:
		fprintf(stderr, "libnetconf DEBUG: %s\n", msg);
		break;
	}
}

void clb_error_print(const char* tag,
		const char* type,
		const char* severity,
		const char* UNUSED(apptag),
		const char* UNUSED(path),
		const char* message,
		const char* UNUSED(attribute),
		const char* UNUSED(element),
		const char* UNUSED(ns),
		const char* UNUSED(sid))
{
	fprintf(stderr, "NETCONF %s: %s (%s) - %s\n", severity, tag, type, message);
}

void usage(char* progname)
{
	printf("Get NETCONF configuration and state data from the NETCONF server.\n\n");
	printf("Usage: %s [-h] [-f \"<filter>\"] [-p <port>] [-l <user>] [-c] [hostname]\n", progname);
	printf("-f \"<filter>\"  Apply NETCONF subtree filter. Remember to correctly escape the argument.\n");
	printf("-h             Show this help\n");
	printf("-p <port>      Connect to a specific port, 830 is default port\n");
	printf("-l <user>      Connect as a specific user, current user is used by default\n");
	printf("-v             Verbose mode\n");
	printf("-c 	       get-config mode\n\n");
	printf("Hostname is a domain name or IP address of the NETCONF server, \'localhost\' is a default value.\n\n");
}

int main(int argc, char* argv[])
{
	int ret = EXIT_SUCCESS;
	NC_VERB_LEVEL verbose = NC_VERB_WARNING;
	struct nc_session *session;
	struct nc_filter *filter = NULL;
	char* host = "localhost";
	unsigned short port = 830;
	char* user = NULL;
	char* data = NULL;
	nc_rpc *rpc = NULL;
	nc_reply *reply = NULL;
	NC_REPLY_TYPE reply_type;
	int c;
	int f_flag_set = 0;
	int get_config_flag=0;
	char *filter_string;

	while ((c = getopt(argc, argv, ARGUMENTS)) != -1) {
		switch (c) {
		case 'f':
			f_flag_set = 1;
			optind = optind+1;
			printf("Enter filter subtree\n");
			scanf("%ms \n",&filter_string);
			filter = nc_filter_new(NC_FILTER_SUBTREE,filter_string);
			break;

		case 'h': /* Show help */
			usage(argv[0]);
			return EXIT_SUCCESS;

		case 'p': /* port */
			port = (unsigned short) atoi (optarg);
			break;

		case 'l': /* login */
			user = optarg;
			break;

		case 'v': /* Verbose operation */
			verbose = NC_VERB_VERBOSE;
			break;

		case 'd':
			verbose = NC_VERB_DEBUG;
			break;
		case 'c':
			get_config_flag=1;
			break;

		default:
			fprintf(stderr, "unknown argument -%c", optopt);
			break;
		}
	}
	if (argc == optind) {
		/* no host specified */
	} else if (((argc - optind) == 1))    {
		host = argv[optind];
	} 
	  else {
		fprintf(stderr, "stray arguments\n");
		return (EXIT_FAILURE);
	}

	/* set verbosity and function to print libnetconf's messages */
	nc_verbosity(verbose);
	nc_callback_print(clb_print);
	nc_callback_error_reply(clb_error_print);

	/* create NETCONF session */
	fprintf(stdout, "Connecting to port %d at %s as %s%s.\n",
			port,
			host,
			user ? "user " : "",
			user ? user : "current user");
	session = nc_session_connect(host, port, user, NULL);
	if (session == NULL) {
		fprintf(stderr, "Connecting to the NETCONF server failed.\n");
		return (EXIT_FAILURE);
	}

	/*  prepare <get> message */
	if(get_config_flag == 0){
		fprintf(stdout,"Preparing <get> message");
		rpc = nc_rpc_get(filter);
	}
	else
	{
		fprintf(stdout,"Preparing <get-config> message");
		rpc = nc_rpc_getconfig(NC_DATASTORE_RUNNING,filter);
	}
	if (rpc == NULL ) {
		fprintf(stderr, "Creating <get> RPC message failed.\n");
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	/* send <rpc> and receive <rpc-reply> */
	switch (nc_session_send_recv(session, rpc, &reply)) {
	case NC_MSG_UNKNOWN:
		fprintf(stderr, "Sending/Receiving NETCONF message failed.\n");
		ret = EXIT_FAILURE;
		goto cleanup;

	case NC_MSG_NONE:
		/* error occurred, but processed by callback */
		goto cleanup;

	case NC_MSG_REPLY:
		if ((reply_type = nc_reply_get_type(reply)) == NC_REPLY_DATA) {
			fprintf(stdout, "%s\n", data = nc_reply_get_data(reply));
			free(data);
		} else {
			fprintf(stderr, "Unexpected type of message received (%d).\n", reply_type);
			ret = EXIT_FAILURE;
			goto cleanup;
		}
		break;

	default:
		fprintf(stderr, "Unknown error occurred.\n");
		ret = EXIT_FAILURE;
		goto cleanup;
	}

cleanup:
	/* free messages */
	nc_rpc_free(rpc);
	nc_reply_free(reply);

	/* free created filter */
	nc_filter_free(filter);

	/* close NETCONF session */
	nc_session_free(session);

	return (ret);
}


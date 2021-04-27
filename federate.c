#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <helics/chelics.h>

static const char defmessageTarget[] = "fed";
static const char defvalueTarget[] = "fed";
static const char defTargetEndpoint[] = "endpoint";
static const char defSourceEndpoint[] = "endpoint";

int main(int argc, char *argv[])
{
  helics_federate_info fedinfo = helicsCreateFederateInfo();
  const char *messagetarget = defmessageTarget;
  const char *valuetarget = defvalueTarget;
  const char *endpoint = defTargetEndpoint;
  const char *source = defSourceEndpoint;

  char *targetEndpoint = NULL;
  char *targetSubscription = NULL;
  int ii;

  helics_federate cFed = NULL;
  helics_endpoint ept = NULL;
  helics_publication pubid = NULL;
  helics_input subid = NULL;

  const char *str;
  char message[1024];
  helics_time newTime;

  for (ii = 1; ii < argc; ++ii) {
    if (strcmp(argv[ii], "--target") == 0) {
      valuetarget = argv[ii + 1];
      messagetarget = argv[ii + 1];
      ++ii;
    } else if (strcmp(argv[ii], "--valuetarget") == 0) {
      valuetarget = argv[ii + 1];
      ++ii;
    } else if (strcmp(argv[ii], "--messagetarget") == 0) {
      messagetarget = argv[ii + 1];
      ++ii;
    } else if (strcmp(argv[ii], "--endpoint") == 0) {
      endpoint = argv[ii + 1];
      ++ii;
    } else if (strcmp(argv[ii], "--source") == 0) {
      source = argv[ii + 1];
      ++ii;
    } else if ((strcmp(argv[ii], "--help") == 0) || (strcmp(argv[ii], "-?") == 0)) {
      printf(" --messagetarget <target federate name>  ,the name of the federate to send messages to\n");
      printf(" --valuetarget <target federate name>  ,the name of the federate to get values from to\n");
      printf(" --target <target federate name>, set the value and message targets to be the same\n");
      printf(" --endpoint <target endpoint name> , the name of the endpoint to send message to\n");
      printf(" --source <endpoint>, the name of the source endpoint to create\n");
      printf(" --help, -? display help\n");

      return 0;
    }
  }

  helicsFederateInfoLoadFromArgs(fedinfo, argc, (const char *const *)argv, NULL);

  cFed = helicsCreateCombinationFederate("fed", fedinfo, NULL);

  targetEndpoint = (char *)malloc(strlen(messagetarget) + 2 + strlen(endpoint));

  strcpy(targetEndpoint, messagetarget);
  strcat(targetEndpoint, "/");
  strcat(targetEndpoint, endpoint);

  str = helicsFederateGetName(cFed);

  printf("registering endpoint %s for %s\n", source, str);

  /*this line actually creates an endpoint*/
  ept = helicsFederateRegisterEndpoint(cFed, source, "", NULL);

  pubid = helicsFederateRegisterTypePublication(cFed, "pub", "double", "", NULL);

  targetSubscription = (char *)malloc(strlen(valuetarget) + 4);

  strcpy(targetSubscription, messagetarget);
  strcat(targetSubscription, "/pub");

  subid = helicsFederateRegisterSubscription(cFed, targetSubscription, "", NULL);

  printf("entering init Mode\n");
  helicsFederateEnterInitializingMode(cFed, NULL);

  printf("entered init Mode\n");
  helicsFederateEnterExecutingMode(cFed, NULL);

  printf("entered execution Mode\n");

  for (ii = 1; ii < 10; ++ii) {
    snprintf(message, 1024, "message sent from %s to %s at time %d", str, targetEndpoint, ii);
    helicsEndpointSendMessageRaw(ept, targetEndpoint, message, (int)(strlen(message)), NULL);

    printf(" %s \n", message);
    helicsPublicationPublishDouble(pubid, (double)ii, NULL);

    newTime = helicsFederateRequestTime(cFed, (helics_time)ii, NULL);

    printf("granted time %f\n", newTime);
    while (helicsEndpointHasMessage(ept) == helics_true) {
      helics_message nmessage = helicsEndpointGetMessage(ept);
      printf("received message from %s at %f ::%s\n", nmessage.source, nmessage.time, nmessage.data);
    }

    if (helicsInputIsUpdated(subid)) {
      double val = helicsInputGetDouble(subid, NULL);
      printf("received updated value of %f at %f from %s\n", val, newTime, targetSubscription);
    }
  }

  printf("finalizing federate\n");
  helicsFederateDestroy(cFed);

  return 0;
}
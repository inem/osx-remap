#include <Carbon/Carbon.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

struct timeval last_event_1 = { 0, 0 }, last_event_2 = { 0, 0 };
int exit_code = 0;

struct flag_mapping {
  CGEventFlags bit;
  char *name;
};

void stop_loop() {
  CFRunLoopStop(CFRunLoopGetCurrent());
}

void bad_socket(char *where) {
  printf("bad socket on %s.. quitting\n", where);
  exit_code = 2;
  stop_loop();
}

void remap_keys(CGEventRef event, CGKeyCode keycode, CGEventFlags flags, char *process_name) {
  CGKeyCode new_keycode;
  CGEventFlags new_flags;
  char tmp[1024];

  struct sockaddr_in sin = { .sin_family = AF_INET, .sin_port = htons(9999), .sin_addr = { .s_addr = inet_addr("127.0.0.1") } };
  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), ret, len = 0;

  if (s < 0) return bad_socket("socket");
  if (connect(s, &sin, sizeof(sin)) < 0) return bad_socket("connect");

  sprintf(tmp, "%hu %lu %s\n", keycode, flags, process_name);
  if (write(s, tmp, strlen(tmp)) < 0) return bad_socket("write");

  memset(tmp, 0, sizeof(tmp));
  while (ret > 0 && !strstr(tmp, "\n")) {
    ret = read(s, &tmp[len], sizeof(tmp)-len);
    len += ret;
  }
  tmp[len] = 0;

  if (ret < 0) return bad_socket("read");

  close(s);

  if (ret > 0) {
    sscanf(tmp, "%hu %lu %s\n", &new_keycode, &new_flags);
    printf("[%hu/%lu] -> [%hu/%lu]\n", keycode, flags, new_keycode, new_flags);
    CGEventSetFlags(event, new_flags);
    CGEventSetIntegerValueField(event, kCGKeyboardEventKeycode, new_keycode);
  }
}

bool force_exit = 0;
void check_times() {
  if (force_exit) {
    printf("forced exit...\n");
    exit(1);
  }

  long double t1 = last_event_1.tv_sec + ((long double)last_event_1.tv_usec/(long double)1000000.0);
  long double t2 = last_event_2.tv_sec + ((long double)last_event_2.tv_usec/(long double)1000000.0);

  printf("time diff %Lf\n", fabsl(t1-t2));
  // more than a 0.1 second delay between events, highly likely that one listener is dead
  if (fabsl(t1 - t2) > 0.1) {
    stop_loop();
    // force exit on next timer in case the stupid loop doesn't actually stop
    force_exit = 1;
  }
}

CGEventRef event_handler(CGEventTapProxy proxy, CGEventType ev_type, CGEventRef event, void *data) {
  gettimeofday(&last_event_1, NULL);

  CGEventSourceRef source;
  
  CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
  CGEventFlags flags = CGEventGetFlags(event);

  CFStringRef cfs;
  char process_name[1024] = {0};
  ProcessSerialNumber p;

  if (GetFrontProcess(&p) == noErr) {
    CopyProcessName(&p, &cfs);
    CFStringGetCString(cfs, process_name, sizeof(process_name), 0);
    CFRelease(cfs);
  }

  remap_keys(event, keycode, flags, process_name);

  return event;
}

CGEventRef event_handler2(CGEventTapProxy proxy, CGEventType ev_type, CGEventRef event, void *data) {
  gettimeofday(&last_event_2, NULL);
  // TODO; get rid of this once it works
  printf(".");fflush(stdout); return event;
}

create_and_run_loop() {
  CFMachPortRef port, port2, port3;
  CFRunLoopSourceRef source, source2, source3;

  if (!(port = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                                (CGEventMaskBit(kCGEventKeyDown)|CGEventMaskBit(kCGEventFlagsChanged)),
                                 event_handler, NULL))) {
    printf("error creating tap\n"); exit(1);
  }

  if (!(source = CFMachPortCreateRunLoopSource(NULL, port, 0))) {
    printf("error creating source\n"); exit(1);
  }

  if (!(port2 = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                                (CGEventMaskBit(kCGEventKeyDown)|CGEventMaskBit(kCGEventFlagsChanged)),
                                 event_handler2, NULL))) {
    printf("error creating tap\n"); exit(1);
  }

  if (!(source2 = CFMachPortCreateRunLoopSource(NULL, port2, 0))) {
    printf("error creating source\n"); exit(1);
  }

  CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), source2, kCFRunLoopDefaultMode);

  CFRelease(port);
  CFRelease(source);
  CFRelease(port2);
  CFRelease(source2);

  CFRunLoopRun();
}

void sig_handler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    exit_code = 2;
    stop_loop();
  }
  if (sig == SIGALRM) check_times();
}

int main() {
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGALRM, sig_handler);

  struct itimerval it_old, it_val = { { 0, 500000 }, { 0, 500000 } };
  
  setitimer(ITIMER_REAL, &it_val, &it_old);

  create_and_run_loop();

  printf("exiting...\n");
  
  return exit_code;
}

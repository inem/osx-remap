#include <Carbon/Carbon.h>

CGEventRef event_handler(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *data) {
  printf(".");fflush(stdout);
  return event;
}

main() {
  CFMachPortRef port;
  CFRunLoopSourceRef source;

  if (!(port = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                                (CGEventMaskBit(kCGEventKeyDown)|CGEventMaskBit(kCGEventFlagsChanged)),
                                 event_handler, NULL))) {
    printf("error creating tap\n"); exit(1);
  }

  if (!(source = CFMachPortCreateRunLoopSource(NULL, port, 0))) {
    printf("error creating source\n"); exit(1);
  }

  CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
  CFRelease(port);
  CFRelease(source);

  CFRunLoopRun();
}

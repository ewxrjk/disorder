/*
 * This file is part of DisOrder
 * Copyright (C) 2009 Richard Kettlewell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/** @file lib/coreaudio.c
 * @brief Support for Apple Core Audio
 */

#include "common.h"

#if HAVE_COREAUDIO_AUDIOHARDWARE_H

#include "coreaudio.h"
#include "log.h"
#include "printf.h"
#include <CoreFoundation/CFString.h>
/* evil bodge to work around broken header file */
#undef error
#include <CoreServices/CoreServices.h>
#include <stdarg.h>

/** @brief Report an error with an OSStatus */
void coreaudio_fatal(OSStatus err, const char *fmt, ...) {
  va_list ap;
  char *msg;

  va_start(ap, fmt);
  byte_vasprintf(&msg, fmt, ap);
  va_end(ap);

  disorder_fatal(0, "%s: error %u", msg, (unsigned)err);
}

/** @brief Return the default device ID */
static AudioDeviceID coreaudio_use_default(void) {
  OSStatus status;
  UInt32 propertySize;
  AudioDeviceID adid;

  /* TODO should we use kAudioHardwarePropertyDefaultSystemOutputDevice
   * instead?  It is listed in the enumeration but is not documented, so we
   * leave it alone until better information is available. */
  propertySize = sizeof adid;
  status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                    &propertySize, &adid);
  if(status)
    coreaudio_fatal(status, "AudioHardwareGetProperty kAudioHardwarePropertyDefaultOutputDevice");
  if(adid == kAudioDeviceUnknown)
    disorder_fatal(0, "no output device");
  return adid;
}

/** @brief Find a device by some string
 * @param selector Selector for property to look for
 * @param devs List of device IDs
 * @param ndevs Number of device IDs in @p devs
 * @param dev Desired device name
 * @param resultp Where to put device ID
 * @return 1 if found, 0 if not found
 */
static int coreaudio_find_device(AudioObjectPropertySelector selector,
                                 //const char *description,
                                 const AudioDeviceID *devs,
                                 unsigned ndevs,
                                 CFStringRef dev,
                                 AudioDeviceID *resultp) {
  OSStatus status;
  UInt32 propertySize;

  for(unsigned n = 0; n < ndevs; ++n) {
    CFStringRef name;
    propertySize = sizeof name;
    status = AudioDeviceGetProperty(devs[n], 0, FALSE,
                                    selector,
                                    &propertySize, &name);
    if(status)
      coreaudio_fatal(status, "AudioDeviceGetProperty");
#if 0
    UInt8 output[1024];
    CFIndex used;
    CFRange r = { 0, CFStringGetLength(name) };
    CFStringGetBytes(name, r, kCFStringEncodingUTF8,
                     '?', FALSE, output, sizeof output,
                     &used);
    output[used] = 0;
    disorder_info("device %u %s: %s", n, description, (char *)output);
#endif
    if(CFStringCompare(dev, name,
                       kCFCompareCaseInsensitive|kCFCompareNonliteral)
       == kCFCompareEqualTo) {
      *resultp = devs[n];
      CFRelease(name);
      return 1;
    }
    CFRelease(name);
  }
  return 0;                             /* didn't find it */
}

/** @brief Identify audio device
 * @param name Device name
 * @return Device ID
 */
AudioDeviceID coreaudio_getdevice(const char *name) {
  OSStatus status;
  UInt32 propertySize;
  int found;
  AudioDeviceID adid;

  if(!name
     || !*name
     || !strcmp(name, "default"))
    return coreaudio_use_default();
  /* Convert the configured device name to a CFString */
  CFStringRef dev;
  dev = CFStringCreateWithCString(NULL/*default allocator*/,
                                  name,
                                  kCFStringEncodingUTF8);
  if(!dev)
    disorder_fatal(0, "CFStringCreateWithBytes failed");
  /* Get a list of available devices */
  AudioDeviceID devs[1024];
  unsigned ndevs;

  propertySize = sizeof devs;
  status = AudioHardwareGetProperty(kAudioHardwarePropertyDevices,
                                    &propertySize, devs);
  if(status)
    coreaudio_fatal(status, "AudioHardwareGetProperty kAudioHardwarePropertyDevices");
  ndevs = propertySize / sizeof *devs;
  if(!ndevs)
    disorder_fatal(0, "no sound devices found");
  /* Try looking up by UID first */
  found = coreaudio_find_device(-1*kAudioDevicePropertyDeviceUID, //"UID",
                                devs, ndevs, dev, &adid);
  /* Failing that try looking up by name */
  if(!found)
    found = coreaudio_find_device(kAudioObjectPropertyName, //"name",
                                  devs, ndevs, dev, &adid);
  CFRelease(dev);
  if(!found)
    disorder_fatal(0, "cannot find device '%s'", name);
  return adid;
}

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/

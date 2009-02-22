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
 * @brief Support for @ref BACKEND_COREAUDIO
 */

#include "common.h"

#if HAVE_COREAUDIO_AUDIOHARDWARE_H

#include "coreaudio.h"
#include "log.h"
#include <CoreFoundation/CFString.h>

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
    fatal(0, "AudioHardwareGetProperty kAudioHardwarePropertyDefaultOutputDevice: %d", (int)status);
  if(adid == kAudioDeviceUnknown)
    fatal(0, "no output device");
  return adid;
}

/** @brief Find a device by some string
 * @param selector Selector for property to look for
 * @param description Property description
 * @param devs List of device IDs
 * @param ndevs Number of device IDs in @p devs
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
      fatal(0, "AudioDeviceGetProperty: %d", (int)status);
#if 0
    UInt8 output[1024];
    CFIndex used;
    CFRange r = { 0, CFStringGetLength(name) };
    CFStringGetBytes(name, r, kCFStringEncodingUTF8,
                     '?', FALSE, output, sizeof output,
                     &used);
    output[used] = 0;
    info("device %u %s: %s", n, description, (char *)output);
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
    fatal(0, "CFStringCreateWithBytes failed");
  /* Get a list of available devices */
  AudioDeviceID devs[1024];
  unsigned ndevs;

  propertySize = sizeof devs;
  status = AudioHardwareGetProperty(kAudioHardwarePropertyDevices,
                                    &propertySize, devs);
  if(status)
    fatal(0, "AudioHardwareGetProperty kAudioHardwarePropertyDevices: %d",
          (int)status);
  ndevs = propertySize / sizeof *devs;
  if(!ndevs)
    fatal(0, "no sound devices found");
  /* Try looking up by UID first */
  found = coreaudio_find_device(kAudioDevicePropertyDeviceUID, //"UID",
                                devs, ndevs, dev, &adid);
  /* Failing that try looking up by name */
  if(!found)
    found = coreaudio_find_device(kAudioObjectPropertyName, //"name",
                                  devs, ndevs, dev, &adid);
  CFRelease(dev);
  if(!found)
    fatal(0, "cannot find device '%s'", name);
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

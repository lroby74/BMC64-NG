









// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef USBLOG_H
#define USBLOG_H

#ifdef __cplusplus
extern "C" {
#endif




void usblog_avvio(void);



void usblog_riga(const char *formato, ...);



void usblog_elenco(const char *motivo);

#ifdef __cplusplus
}
#endif

#endif

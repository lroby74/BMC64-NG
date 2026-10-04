
















// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRTPROVA_H
#define CRTPROVA_H

#ifdef __cplusplus
extern "C" {
#endif



const char *crtprova_modo(const char *nome);


const char *crtprova_nome(int i);




int crtprova_esegui(const char *nome_o_timings, int secondi);

#ifdef __cplusplus
}
#endif

#endif

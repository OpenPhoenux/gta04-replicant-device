/*
 * Copyright (C) 2016 Lukas Märdian <lukas@goldelico.com>
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _GTA04_H_
#define _GTA04_H_

extern int get_model_name(char* buf);
extern int model_contains_str(char* buf);
extern int is_gta04a3();
extern int is_gta04a4();
extern int is_gta04a5();

#endif

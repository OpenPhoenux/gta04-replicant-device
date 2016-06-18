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

#define LOG_TAG "gta04_device"
#define TRUE 1
#define FALSE 0
#include <cutils/log.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char model_path[] =
	"/sys/firmware/devicetree/base/model";

int get_model_name(char* buf) {
	char str[500];
	FILE *f;
	if ((f=fopen(model_path,"r"))==NULL){
		ALOGE("Error opening %s", model_path);
		return -1;
	}
	fscanf(f,"%[^\n]", buf);
	fclose(f);
	return 0;
}

int model_contains_str(char* buf) {
	char model_name[500];
	if(get_model_name(model_name) == 0) {
		if(strstr(model_name, buf) != NULL) {
			return TRUE;
		}
	}
	return FALSE;
}

int is_gta04a3() {
	return model_contains_str("GTA04A3");
}

int is_gta04a4() {
	return model_contains_str("GTA04A4");
}

int is_gta04a5() {
	return model_contains_str("GTA04A5");
}

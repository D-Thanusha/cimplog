/**
 * Copyright 2016 Comcast Cable Communications Management, LLC
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
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cimplog.h"

#define MAX_BUF_SIZE 1024

struct mosquitto *mosq = NULL;
char* clientId;
char* broker;
char *hostip = NULL;
int port = 443;

#if defined(LEVEL_DEFAULT)
int cimplog_debug_level = LEVEL_DEFAULT;
#else
int cimplog_debug_level = LEVEL_INFO;
#endif

void __cimplog(const char *module, int level, const char *msg, ...)
{
    static const char *_level[] = { "Error", "Info", "Debug", "Unknown" };
    va_list arg_ptr;
    char buf[MAX_BUF_SIZE];
    int nbytes;
    struct timespec ts;

    if (level <= cimplog_debug_level)
    {
        va_start(arg_ptr, msg);
        nbytes = vsnprintf(buf, MAX_BUF_SIZE, msg, arg_ptr);
        va_end(arg_ptr);

        if( nbytes >=  MAX_BUF_SIZE )	
        {
            buf[ MAX_BUF_SIZE - 1 ] = '\0';
        }
        else
        {
            buf[nbytes] = '\0';
        }
    
        clock_gettime(CLOCK_REALTIME, &ts);

        printf("[%09ld][%s][%s]: %s", ts.tv_sec, module, _level[0x3 & level], buf);
    }
}

void __cimplog_generic(const char *module, const char *msg, ...)
{
    (void) module;
    (void) msg;
}

int (*disconnectCbFn)(int) = NULL;

int RegisterCB(disconnectCb cb)
{
	disconnectCbFn = cb;
	return 1;
}

void get_from_file(char *key, char **val, char *filepath)
{
        FILE *fp = fopen(filepath, "r");

        if (NULL != fp)
        {
                char str[255] = {'\0'};
                while (fgets(str, sizeof(str), fp) != NULL)
                {
                    char *value = NULL;

                    if(NULL != (value = strstr(str, key)))
                    {
                        value = value + strlen(key);
                        value[strlen(value)-1] = '\0';
                        *val = strdup(value);
                        break;
                    }

                }
                fclose(fp);
        }

        if (NULL == *val)
        {
                printf("val is not present in file\n");

        }
        else
        {
                printf("val fetched is %s\n", *val);
        }
}

int connectMqttBroker()
{
	printf("------------ConnectMqttBroker--------------\n");
	int clean_session = false;
	int rc = 0;
	clientId = "C089ABEEA4BE";
	printf("clientId - %s\n", clientId);
	//strcpy(port, "443");
	broker = "hcbroker-chi-02-pub.staging.us-west-2.plume.comcast.net";
	printf("broker - %s\n", broker);
	mosquitto_lib_init();
	printf("mosquitto init success\n");
	if(clientId !=NULL)
	{
		mosq = mosquitto_new(clientId, clean_session, NULL);
	}
	else
	{
		printf("clientId is NULL, init with clean_session true\n");
		mosq = mosquitto_new(NULL, true, NULL);
	}
	if(!mosq)
	{
		printf("Error initializing mosq instance\n");
		return MOSQ_ERR_NOMEM;
	}
	mosquitto_int_option(mosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);
	struct libmosquitto_tls *tls;
	tls = malloc (sizeof (struct libmosquitto_tls));
	if(tls)
	{
		memset(tls, 0, sizeof(struct libmosquitto_tls));

		char * CAFILE, *CERTFILE , *KEYFILE = NULL;

		get_from_file("CA_FILE_PATH=", &CAFILE, MQTT_CONFIG_FILE);
		get_from_file("CERT_FILE_PATH=", &CERTFILE, MQTT_CONFIG_FILE);
		get_from_file("KEY_FILE_PATH=", &KEYFILE, MQTT_CONFIG_FILE);

		if(CAFILE !=NULL && CERTFILE!=NULL && KEYFILE !=NULL)
		{
			printf("CAFILE %s, CERTFILE %s, KEYFILE %s MOSQ_TLS_VERSION %s\n", CAFILE, CERTFILE, KEYFILE, MOSQ_TLS_VERSION);

			tls->cafile = CAFILE;
			tls->certfile = CERTFILE;
			tls->keyfile = KEYFILE;
			tls->tls_version = MOSQ_TLS_VERSION;

			rc = mosquitto_tls_set(mosq, tls->cafile, tls->capath, tls->certfile, tls->keyfile, tls->pw_callback);
			printf("mosquitto_tls_set rc %d\n", rc);
			if(rc)
			{
				printf("Failed in mosquitto_tls_set %d %s\n", rc, mosquitto_strerror(rc));
			}
			else
			{
				rc = mosquitto_tls_opts_set(mosq, tls->cert_reqs, tls->tls_version, tls->ciphers);
				printf("mosquitto_tls_opts_set rc %d\n", rc);
				if(rc)
				{
					printf("Failed in mosquitto_tls_opts_set %d %s\n", rc, mosquitto_strerror(rc));
				}
			}

		}
		else
		{
			printf("Failed to get tls cert files\n");
			rc = 1;
		}
		if(rc == MOSQ_ERR_SUCCESS)
		{
			//connect to mqtt broker
			mosquitto_disconnect_v5_callback_set(mosq, on_disconnect);
			
			printf("port %d\n", port);
			rc = mosquitto_connect_bind_v5(mosq, broker, port, KEEPALIVE, hostip, NULL);
			printf("mosquitto_connect_bind rc %d\n", rc);
			if(rc != MOSQ_ERR_SUCCESS)
			{
				printf("mqtt connect Error: %s\n", mosquitto_strerror(rc));
				mosquitto_destroy(mosq);

				MQTTCM_FREE(CAFILE);
				MQTTCM_FREE(CERTFILE);
				MQTTCM_FREE(KEYFILE);
				return rc;
							
			}
			else
			{
				printf("mqtt broker connect success %d\n", rc);
		
			}
			printf("mosquitto_loop_forever\n");
			rc = mosquitto_loop_forever(mosq, -1, 1);
			if(rc != MOSQ_ERR_SUCCESS)
			{
				mosquitto_destroy(mosq);
				printf("mosquitto_loop_start Error: %s\n", mosquitto_strerror(rc));

					MQTTCM_FREE(CAFILE);
					MQTTCM_FREE(CERTFILE);
					MQTTCM_FREE(KEYFILE);
					return rc;
			}
			else
			{
					mosquitto_destroy(mosq);
					mosquitto_lib_cleanup();
			}
		}
		else
		{
			printf("Mqtt tls cert failed!!!, Abort the process\n");
			mosquitto_destroy(mosq);
		}
	}
	else
	{
		printf("Allocation failed\n");
		rc = MOSQ_ERR_NOMEM;
	}
	return rc;
}

void on_disconnect(struct mosquitto *mosq, void *obj, int reason_code, const mosquitto_property *props)
{
        printf("mqtt on_disconnect: reason_code %d\n", reason_code);
	(*disconnectCbFn)(reason_code);
}

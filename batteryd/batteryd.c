/*
 * Modified from http://phhusson.free.fr/miniprogs/uevent.c.html (WTFPL License)
 */

typedef unsigned short int sa_family_t;
#define __KERNEL_STRICT_NAMES
#include <sys/types.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

//From a uevent patch for hal
#define HOTPLUG_BUFFER_SIZE             1024
#define HOTPLUG_NUM_ENVP                32
#define OBJECT_SIZE                     512

int main(int argc, char **argv, char **envp) {
        //Start listening
        int fd;
        int ret;
        FILE *id_fd;
        FILE *max_current_fd;
        char id_status[80];
        char *max_current;
        char *max_current_500000 = "500000";
        char *max_current_851000 = "851000";
        struct sockaddr_nl ksnl;
        memset(&ksnl, 0x00, sizeof(struct sockaddr_nl));
        ksnl.nl_family=AF_NETLINK;
        ksnl.nl_pid=getpid();
        ksnl.nl_groups=0xffffffff;
        fd=socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
        if (fd==-1) {
                printf("Couldn't open kobject-uevent netlink socket");
                perror("");
                exit(1);
        }
        if (bind(fd, (struct sockaddr *) &ksnl, sizeof(struct sockaddr_nl))<0) {
                fprintf (stderr, "Error binding to netlink socket");
                close(fd);
                exit(1);
        }

        while(1) {
                char buffer[HOTPLUG_BUFFER_SIZE + OBJECT_SIZE];
                int buflen;
                buflen=recv(fd, &buffer, sizeof(buffer), 0);
                if (buflen<0) {
                        exit(1);
                }
                //printf("%s\n", buffer);
                ret = strncmp(buffer, "change@/devices/platform/omap_i2c.1/i2c-1/1-004a/twl4030_bci/power_supply/twl4030_usb", 85);
                /* USB/Charger plugged or unplugged */
                if(ret == 0) {
                    id_fd = fopen("/sys/devices/platform/omap_i2c.1/i2c-1/1-0048/twl4030_usb/id","r");
                    fscanf(id_fd, "%s", &id_status[0]); //read content of id_fd
                    fclose(id_fd);

                    //id: unknown, floating, GND, 102k
                    printf("uevent change@twl4030_usb (id:%s)\n", id_status);

                    if(strncmp(id_status, "floating", 8) == 0)
                        max_current = max_current_500000;
                    else if(strncmp(id_status, "102k", 4) == 0)
                        max_current = max_current_851000;
                    else if(strncmp(id_status, "GND", 3) == 0)
                        max_current = max_current_851000;
                    else
                        max_current = max_current_500000;

                    max_current_fd = fopen("/sys/devices/platform/omap_i2c.1/i2c-1/1-004a/twl4030_bci/max_current","w");
                    fprintf(max_current_fd, "%s\n", max_current); //write max_current
                    fclose(max_current_fd);
                }
        }
}


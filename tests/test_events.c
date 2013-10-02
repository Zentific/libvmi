/* The LibVMI Library is an introspection library that simplifies access to 
 * memory in a target virtual machine or in a file containing a dump of 
 * a system's physical memory.  LibVMI is based on the XenAccess Library.
 *
 * Copyright 2012 VMITools Project
 *
 * Author: Steven Maresca (steve@zentific.com)
 *
 * This file is part of LibVMI.
 *
 * LibVMI is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * LibVMI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with LibVMI.  If not, see <http://www.gnu.org/licenses/>.
 */

#if ENABLE_XEN_EVENTS

#include <check.h>
#include "../libvmi/libvmi.h"
#include "check_tests.h"
#include <time.h>

#define TEST_TIMEOUT 30 /* seconds */

bool event_received = 0;
static int interrupted = 0;

static void close_handler(int sig){
    interrupted = sig;
}

void event_received_callback(vmi_instance_t vmi, vmi_event_t *event){
    event_received = 1;
}

/* Test registration and receipt of control register writes
 *
 *  Use CR3 as a test case; if this works, CR0 and CR4 monitoring should work
 *  also, as they are implemented via trivial variation in the switch-case
 *  statement)
 */
START_TEST (test_libvmi_cr_write_event)
{
    vmi_instance_t vmi = NULL;
    vmi_event_t cr3_event = {0};
    struct sigaction act;

    interrupted = 0;
    event_received = 0;

    /* for a clean exit ... not really sure how this will interact with
     *  check's signal handling stuff across forks
     */
    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    status_t e = vmi_init(&vmi, VMI_XEN | VMI_INIT_COMPLETE | VMI_INIT_EVENTS, get_testvm());

    if(e == VMI_FAILURE){
        ck_abort_msg("CR write test failed: could not initialize LibVMI");
    }

    /* Configure an event to track when the process is running.
     * (The CR3 register is updated on task context switch, allowing
     *  us to follow as various tasks are scheduled and run upon the CPU)
     */
    cr3_event.type = VMI_EVENT_REGISTER;
    cr3_event.reg_event.reg = CR3;

    /* Observe only write events to the given register. 
     *   NOTE: read events are unsupported at this time.
     */
    cr3_event.reg_event.in_access = VMI_REGACCESS_W;

    /* callback fired when event is encountered */
    cr3_event.callback = event_received_callback;

    /* register the configured event */
    vmi_register_event(vmi, &cr3_event);

    time_t start, now;
    now = start = time(NULL);

    while(!interrupted && (now - start) < TEST_TIMEOUT){
        status = vmi_events_listen(vmi,500);

        now = time(NULL);

        if (status != VMI_SUCCESS) {
            interrupted = -1;
        }
    }

    vmi_destroy(vmi);
    
    fail_if((now - start) >= TEST_TIMEOUT, "CR3 register event test timeout reached");
    fail_if(event_received != 1, "CR3 register test receipt failed");
}
END_TEST

/* Test registration and receipt of interrupt events (INT3 being the 
 *  only one currently available)
 */
START_TEST (test_libvmi_int3_event)
{
    vmi_instance_t vmi = NULL;
    vmi_event_t int3_event = {0};
    struct sigaction act;

    interrupted = 0;
    event_received = 0;

    /* for a clean exit ... not really sure how this will interact with
     *  check's signal handling stuff across forks
     */
    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    status_t e = vmi_init(&vmi, VMI_XEN | VMI_INIT_PARTIAL | VMI_INIT_EVENTS, get_testvm());

    if(e == VMI_FAILURE){
        ck_abort_msg("INT3 event test failed: could not initialize LibVMI");
    }

    /* Configure an event to track when the process is running.
     *  A process with an embedded, intentional int3 instruction
     *  of the form 0xCC is being executed repeatedly in a loop every
     *  10 seconds within the test VM in order to guarantee that
     *  events are generated.
     */
    int3_event.type = VMI_EVENT_INTERRUPT;
    int3_event.interrupt_event.enabled = 1;
    int3_event.interrupt_event.reinject = 1;

    /* callback fired when event is encountered */
    int3_event.callback = event_received_callback;

    /* register the configured event */
    vmi_register_event(vmi, &int3_event);

    time_t start, now;
    now = start = time(NULL);

    while(!interrupted && (now - start) < TEST_TIMEOUT){
        status = vmi_events_listen(vmi,500);

        now = time(NULL);

        if (status != VMI_SUCCESS) {
            interrupted = -1;
        }
    }

    vmi_destroy(vmi);
    
    fail_if((now - start) >= TEST_TIMEOUT, "INT3 interrupt event test timeout reached");
    fail_if(event_received != 1, "INT3 interrupt event test failed");
}
END_TEST

/* translate test cases */
TCase *events_tcase (void)
{
    TCase *tc_events = tcase_create("LibVMI Events");
    tcase_add_test(tc_events, test_libvmi_cr_write_event);
    tcase_add_test(tc_events, test_libvmi_int3_event);
    return tc_events;
}

#endif /* ENABLE_XEN_EVENTS */

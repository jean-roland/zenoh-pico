//
// Copyright (c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <zenoh-pico.h>

#if Z_FEATURE_SUBSCRIPTION == 1
int main(int argc, char **argv) {
    const char *keyexpr = "demo/example/**";
    char *locator = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "k:e:")) != -1) {
        switch (opt) {
            case 'k':
                keyexpr = optarg;
                break;
            case 'e':
                locator = optarg;
                break;
            case '?':
                if (optopt == 'k' || optopt == 'e') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                }
                return 1;
            default:
                return -1;
        }
    }

    z_owned_config_t config;
    z_config_default(&config);
    if (locator != NULL) {
        zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, locator);
    }

    printf("Opening session...\n");
    z_owned_session_t s;
    if (z_open(&s, z_move(config), NULL) < 0) {
        printf("Unable to open session!\n");
        return -1;
    }

    // Start read and lease tasks for zenoh-pico
    if (zp_start_read_task(z_loan_mut(s), NULL) < 0 || zp_start_lease_task(z_loan_mut(s), NULL) < 0) {
        printf("Unable to start read and lease tasks\n");
        z_session_drop(z_session_move(&s));
        return -1;
    }

    printf("Declaring Subscriber on '%s'...\n", keyexpr);
    z_owned_closure_sample_t closure;
    z_owned_fifo_handler_sample_t handler;
    z_fifo_channel_sample_new(&closure, &handler, 3);

    z_owned_subscriber_t sub;
    z_view_keyexpr_t ke;
    if (z_view_keyexpr_from_str(&ke, keyexpr) < 0) {
        printf("%s is not a valid key expression\n", keyexpr);
        return -1;
    }
    if (z_declare_subscriber(z_loan(s), &sub, z_loan(ke), z_move(closure), NULL) < 0) {
        printf("Unable to declare subscriber.\n");
        return -1;
    }

    z_owned_sample_t sample;
    for (z_result_t res = z_recv(z_loan(handler), &sample); res == Z_OK; res = z_recv(z_loan(handler), &sample)) {
        z_view_string_t keystr;
        z_keyexpr_as_view_string(z_sample_keyexpr(z_loan(sample)), &keystr);
        z_owned_string_t value;
        z_bytes_to_string(z_sample_payload(z_loan(sample)), &value);
        printf(">> [Subscriber] Received ('%.*s': '%.*s')\n", (int)z_string_len(z_loan(keystr)),
               z_string_data(z_loan(keystr)), (int)z_string_len(z_loan(value)), z_string_data(z_loan(value)));
        z_drop(z_move(value));
        z_drop(z_move(sample));
    }

    z_drop(z_move(sub));
    z_drop(z_move(handler));

    z_drop(z_move(s));

    return 0;
}
#else
int main(void) {
    printf("ERROR: Zenoh pico was compiled without Z_FEATURE_SUBSCRIPTION but this example requires it.\n");
    return -2;
}
#endif

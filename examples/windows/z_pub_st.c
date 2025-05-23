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
//

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <zenoh-pico.h>

#define N 2147483647  // max int value by default

#if Z_FEATURE_PUBLICATION == 1
int main(int argc, char **argv) {
    (void)(argc);
    (void)(argv);
    const char *keyexpr = "demo/example/zenoh-pico-pub";
    const char *value = "Pub from Pico!";
    const char *mode = "client";
    char *locator = NULL;

    z_owned_config_t config;
    z_config_default(&config);
    zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, mode);
    if (locator != NULL) {
        zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, locator);
    }

    printf("Opening session...\n");
    z_owned_session_t s;
    if (z_open(&s, z_move(config), NULL) < 0) {
        printf("Unable to open session!\n");
        return -1;
    }

    printf("Declaring publisher for '%s'...\n", keyexpr);
    z_owned_publisher_t pub;
    z_view_keyexpr_t ke;
    if (z_view_keyexpr_from_str(&ke, keyexpr) < 0) {
        printf("%s is not a valid key expression\n", keyexpr);
        return -1;
    }
    if (z_declare_publisher(z_loan(s), &pub, z_loan(ke), NULL) < 0) {
        printf("Unable to declare publisher for key expression!\n");
        return -1;
    }
    // Read received declaration
    zp_read(z_loan(s), NULL);

    printf("Press CTRL-C to quit...\n");
    char *buf = (char *)malloc(256);
    z_clock_t now = z_clock_now();
    for (int idx = 0; idx < N;) {
        if (z_clock_elapsed_ms(&now) > 1000) {
            snprintf(buf, 256, "[%4d] %s", idx, value);
            printf("Putting Data ('%s': '%s')...\n", keyexpr, buf);

            // Create payload
            z_owned_bytes_t payload;
            z_bytes_copy_from_str(&payload, buf);

            z_publisher_put(z_loan(pub), z_move(payload), NULL);
            ++idx;

            now = z_clock_now();
        }

        zp_read(z_loan(s), NULL);
        zp_send_keep_alive(z_loan(s), NULL);
        zp_send_join(z_loan(s), NULL);
    }

    z_drop(z_move(pub));

    z_drop(z_move(s));

    free(buf);
    return 0;
}
#else
int main(void) {
    printf("ERROR: Zenoh pico was compiled without Z_FEATURE_PUBLICATION but this example requires it.\n");
    return -2;
}
#endif

// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_THREADUTIL_H
#define BITCOIN_THREADUTIL_H

#include <string>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h> // For PR_GET_NAME
#endif

#if (defined(PR_GET_NAME) || defined(MAC_OSX))
#define CAN_READ_PROCESS_NAME
#endif

namespace thread_util
{
    /**
     * Rename a thread both in terms of an internal (in-memory) name as well
     * as its system process name.
     *
     * @return whether or not the rename succeeded.
     */
    bool rename(std::string);

    /**
     * Set the thread's internal (in-memory) name; used e.g. for identification in
     * logging.
     */
    std::string get_internal_name();

    /**
     * Set the in-memory internal name for this thread. Does not affect the process
     * name.
     */
    bool set_internal_name(std::string);

    /**
     * @return an id unique to this thread.
     */
    long get_internal_id();

    /**
     * @return this thread's name according to the related system process.
     */
    std::string get_process_name();

    /**
     * Set the thread's name at the process level. Does not affect the
     * internal name.
     */
    void set_process_name(const char* name);

} // namespace thread_util

#endif // BITCOIN_THREADUTIL_H

This is a simple process monitor for situations in which you
would like to restart something when it fails but aren't looking
for the complexity of solution such as DaemonTools_, Runit_,
Upstart_, and similar.

You run it like this::

  monitor command [options]

This will go to the background, where monitor will fork off *command* and
wait for it to exit.  If the command exits successfully, ``monitor`` will
exit as well.  If the command exits with an error, ``monitor`` will restart
it.

Monitor uses an exponential backoff mechanism to prevent a failure from
turning into a DOS attack.

Options
=======

- -p *pidfile*

  Write monitor's process id to *pidfile*.  Relative paths will be relative
  to *workdir*.

  The *pidfile* is removed when monitor exits.

- -d *workdir*

  Change to *workdir* before starting your command.

- -f

  Run in the foreground.

- -I *interval*

  If the child process exits before running at least *interval* seconds,
  activate the exponential backoff mechanism.  This will cause monitor to
  sleep for 1 seconds before restarting the command, then 2 seconds, and so
  forth up to *interval* seconds.

  The default *interval* is 60.

Logging
=======

Monitor logs to the syslog ``DAEMON`` facility.  When running the for
foreground, monitor will also log to stderr.

Signals
=======

If monitor receives ``SIGTERM``, it will pass ``SIGTERM`` on to the
child command before exiting.

License
=======

Copyright (c) 2011, Lars Kellogg-Stedman
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

This software includes a soure file from daemonize_, by Brian M. Clapper,
which requires the following notice:

  Copyright 2003-2011 Brian M. Clapper.
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  
  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  
  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  
  * Neither the name "clapper.org" nor the names of its contributors may be
    used to endorse or promote products derived from this software without
    specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

.. _daemontools: http://cr.yp.to/daemontools.html
.. _runit: http://smarden.org/runit/
.. _upstart: http://upstart.ubuntu.com/
.. _daemonize: http://software.clapper.org/daemonize/

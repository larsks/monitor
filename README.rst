This is a simple process monitor for situations in which you
would like to restart something when it fails but aren't looking
for the complexity of solution such as DaemonTools_, Runit_,
Upstart_, and similar.

You run it like this:

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

  Write monitor's process id to *pidfile*.

- -d *dir*

  Change to *dir* before starting your command.

- -f

  Run in the foreground.

Signals
=======

If monitor receives ``SIGTERM``, it will pass ``SIGTERM`` on to the
child command before exiting.


.. _daemontools: http://cr.yp.to/daemontools.html
.. _runit: http://smarden.org/runit/
.. _upstart: http://upstart.ubuntu.com/


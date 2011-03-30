#include <ev.h>

#include <stdio.h> // for puts

// every watcher type has its own typedef'd struct
// with the name ev_TYPE
ev_io stdin_watcher;
ev_timer timeout_watcher;

// all watcher callbacks have a similar signature
// this callback is called when data is readable on stdin
static void
stdin_cb (EV_P_ ev_io *w, int revents)
{
	puts ("stdin ready");
	// for one-shot events, one must manually stop the watcher
	// with its corresponding stop function.
	ev_io_stop (EV_A_ w);

	// this causes all nested ev_loop's to stop iterating
	ev_unloop (EV_A_ EVUNLOOP_ALL);
}

// another callback, this time for a time-out
static void
timeout_cb (EV_P_ ev_timer *w, int revents)
{
	puts ("timeout");
	// this causes the innermost ev_loop to stop iterating
	ev_unloop (EV_A_ EVUNLOOP_ONE);
}

int
main (void)
{
	// use the default event loop unless you have special needs
	struct ev_loop *loop = ev_default_loop (0);

	// initialise an io watcher, then start it
	// this one will watch for stdin to become readable
	ev_io_init (&stdin_watcher, stdin_cb, /*STDIN_FILENO*/ 0, EV_READ);
	ev_io_start (loop, &stdin_watcher);

	// initialise a timer watcher, then start it
	// simple non-repeating 5.5 second timeout
	ev_timer_init (&timeout_watcher, timeout_cb, 5.5, 0.);
	ev_timer_start (loop, &timeout_watcher);

	// now wait for events to arrive
	ev_loop (loop, 0);

	// unloop was called, so exit
	return 0;
}

Coincident
==========
    From WordNet (r) 3.0 (2006) [wn]:

     coincident  
	 adj 1: occurring or operating at the same time; "a series of  
		coincident events" [syn: {coincident}, {coincidental},  
		{coinciding}, {concurrent}, {co-occurrent},  
		{cooccurring}, {simultaneous}]  

Coincident is a race-condition finder that targets unit tests. It can
find both locking problems and data races in C/C++ programs. It works
by re-running a threaded or interrupt driven program multiple times
while varying the thread scheduling, similar to how the Microsoft
CHESS project [1] operates.  In contrast to CHESS, Coincident
schedules threads at memory stores in addition to locking primitives,
and can therefore reliably find data races.

Coincident is meant to be used in unit tests and provides a library
and an API that can be called from the unit test framework. Crpcut [2]
is a good unit test framework which is used in the Coincident
examples, although coincident is general enough to use with any
framework.

To get started, look in the self-test directory for some examples on
how the Coincident library is used.

[1] http://research.microsoft.com/en-us/projects/chess/

[2] http://crpcut.sourceforge.net/

bin_PROGRAMS= \
	portos

portos_SOURCES = \
	$(top_srcdir)/start.c \
	$(top_srcdir)/machineprimitives_x86_64_asm.S \
	$(top_srcdir)/machineprimitives_x86_64.c \
	$(top_srcdir)/machineprimitives.c \
	$(top_srcdir)/random.c \
	$(top_srcdir)/queue.c \
	$(top_srcdir)/synch.c \
	$(top_srcdir)/minithread.c \
	$(top_srcdir)/buffer.c \ # modify this line to compile other tests.
	$(top_srcdir)/end.c

portos_CPPFLAGS = \
	-I$(top_srcdir)/

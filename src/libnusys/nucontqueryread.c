/*
 * Controller status probe.
 *
 * Asks the SI Manager to refresh the connection state and device type of every
 * controller port. The probe runs asynchronously through the SI message path;
 * the results land in the shared nuContStatus table, not in a return value.
 */
#include <nusys.h>

/*
 * Queue a controller-status query on the SI Manager.
 *
 * The query word carries no payload, so the message data pointer is NULL. The
 * scan takes a couple of milliseconds to complete on the SI side, after which
 * nuContStatus reflects which ports are populated and with what device.
 */
void nuContQueryRead(void) { nuSiSendMesg(NU_CONT_QUERY_MSG, (void*)NULL); }

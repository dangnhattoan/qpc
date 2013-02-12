/*****************************************************************************
* Product: QK/C
* Last Updated for Version: 4.4.04
* Date of the Last Update:  Feb 02, 2013
*
*                    Q u a n t u m     L e a P s
*                    ---------------------------
*                    innovating embedded systems
*
* Copyright (C) 2002-2013 Quantum Leaps, LLC. All rights reserved.
*
* This program is open source software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published
* by the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* Alternatively, this program may be distributed and modified under the
* terms of Quantum Leaps commercial licenses, which expressly supersede
* the GNU General Public License and are specifically designed for
* licensees interested in retaining the proprietary status of their code.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
* Contact information:
* Quantum Leaps Web sites: http://www.quantum-leaps.com
*                          http://www.state-machine.com
* e-mail:                  info@quantum-leaps.com
*****************************************************************************/
#include "qk_pkg.h"
/*#include "qassert.h"*/

/*Q_DEFINE_THIS_MODULE("qk_sched")*/

/**
* \file
* \ingroup qk
* \brief QK_schedPrio_() and QK_sched_() implementation.
*/

/*..........................................................................*/
/* NOTE: QK schedPrio_() is entered and exited with interrupts DISABLED     */
uint8_t QK_schedPrio_(void) {
    uint8_t p;               /* highest-priority active object ready to run */

#if (QF_MAX_ACTIVE <= 8)
        /* determine the priority of the highest-priority task ready to run */
    QPSet8_findMax(&QK_readySet_, p);
#else
        /* determine the priority of the highest-priority task ready to run */
    QPSet64_findMax(&QK_readySet_, p);
#endif

#ifdef QK_NO_MUTEX
    if (p <= QK_currPrio_) {                    /* do we have a preemption? */
#else                                  /* QK priority-ceiling mutex allowed */
    if ((p <= QK_currPrio_) || (p <= QK_ceilingPrio_)) {
#endif
        p = (uint8_t)0;
    }
    return p;
}
/*..........................................................................*/
/* NOTE: the QK scheduler is entered and exited with interrupts LOCKED      */
void QK_sched_(uint8_t p) {
    uint8_t pin = QK_currPrio_;                /* save the initial priority */
    QActive *a;
#ifdef QK_TLS                                 /* thread-local storage used? */
    uint8_t pprev = pin;
#endif
    do {
        QEvt const *e;
        a = QF_active_[p];                  /* obtain the pointer to the AO */

        QK_currPrio_ = p;         /* this becomes the current task priority */

#ifdef QK_TLS                                 /* thread-local storage used? */
        if (p != pprev) {                       /* are we changing threads? */
            QK_TLS(a);                   /* switch new thread-local storage */
            pprev = p;
        }
#endif
        QS_BEGIN_NOCRIT_(QS_QK_SCHEDULE, QS_aoObj_, a)
            QS_TIME_();                                        /* timestamp */
            QS_U8_(p);                            /* the priority of the AO */
            QS_U8_(pin);                          /* the preempted priority */
        QS_END_NOCRIT_()

        QF_INT_ENABLE();               /* unconditionally enable interrupts */

        e = QActive_get_(a);              /* get the next event for this AO */
        QF_ACTIVE_DISPATCH_(&a->super, e);            /* dispatch to the AO */
        QF_gc(e);                /* garbage collect the event, if necessary */

        QF_INT_DISABLE();                             /* disable interrupts */

#if (QF_MAX_ACTIVE <= 8)  /* determine the highest-priority AO ready to run */
        QPSet8_findMax(&QK_readySet_, p);
#else
        QPSet64_findMax(&QK_readySet_, p);
#endif

#ifdef QK_NO_MUTEX
    } while (p > pin);          /* is the new priority higher than initial? */
#else                                /* QK priority-ceiling mutexes allowed */
    } while ((p > pin) && (p > QK_ceilingPrio_));
#endif

    QK_currPrio_ = pin;                     /* restore the initial priority */

#ifdef QK_TLS                                 /* thread-local storage used? */
    if (pin != (uint8_t)0) {       /* no extended context for the idle loop */
        a = QF_active_[pin];             /* the pointer to the preempted AO */
        QK_TLS(a);                              /* restore the original TLS */
    }
#endif
}

..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Cavium, Inc

dpdk-test-eventdev Application
==============================

The ``dpdk-test-eventdev`` tool is a Data Plane Development Kit (DPDK)
application that allows exercising various eventdev use cases.
This application has a generic framework to add new eventdev based test cases to
verify functionality and measure the performance parameters of DPDK eventdev
devices.


Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

   dpdk-test-eventdev [EAL Options] -- [application options]

EAL Options
~~~~~~~~~~~

The following are the EAL command-line options that can be used in conjunction
with the ``dpdk-test-eventdev`` application.
See the DPDK Getting Started Guides for more information on these options.

*   ``-l <CORELIST>``

        List the set of cores to use.

*   ``--vdev <driver><id>``

        Add a virtual eventdev device.

Application Options
~~~~~~~~~~~~~~~~~~~

The following are the application command-line options:

* ``--verbose``

        Set verbose level. Default is 1. Value > 1 displays more details.

* ``--dev <n>``

        Set the device id of the event device.

* ``--test <name>``

        Set test name, where ``name`` is one of the following::

         order_queue
         order_atq
         atomic_queue
         atomic_atq
         perf_queue
         perf_atq
         pipeline_atq
         pipeline_queue

* ``--socket_id <n>``

        Set the socket id of the application resources.

* ``--pool-sz <n>``

        Set the number of mbufs to be allocated from the mempool.

* ``--plcores <CORELIST>``

        Set the list of cores to be used as producers.

* ``--wlcores <CORELIST>``

        Set the list of cores to be used as workers.

* ``--stlist <type_list>``

        Set the scheduled type of each stage where ``type_list`` size
        determines the number of stages used in the test application.
        Each type_list member can be one of the following::

            P or p : Parallel schedule type
            O or o : Ordered schedule type
            A or a : Atomic schedule type

        Application expects the ``type_list`` in comma separated form (i.e. ``--stlist o,a,a,a``)

* ``--nb_flows <n>``

        Set the number of flows to produce.

* ``--nb_pkts <n>``

        Set the number of packets to produce. 0 implies no limit.

* ``--worker_deq_depth <n>``

        Set the dequeue depth of the worker.

* ``--fwd_latency``

        Perform forward latency measurement.

* ``--queue_priority``

        Enable queue priority.

* ``--prod_type_ethdev``

        Use ethernet device as producer.

* ``--prod_type_timerdev``

        Use event timer adapter as producer.

* ``--prod_type_timerdev_burst``

       Use burst mode event timer adapter as producer.

* ``--prod_type_cryptodev``

        Use crypto device as producer.

* ``--prod_type_dmadev``

        Use DMA device as producer.

* ``--prod_type_vector``

        Use event vector adapter as producer.

* ``--timer_tick_nsec``

       Used to dictate number of nano seconds between bucket traversal of the
       event timer adapter. Refer `rte_event_timer_adapter_conf`.

* ``--max_tmo_nsec``

       Used to configure event timer adapter max arm timeout in nano seconds.

* ``--expiry_nsec``

       Dictate the number of nano seconds after which the event timer expires.

* ``--nb_timers``

       Number of event timers each producer core will generate.

* ``--nb_timer_adptrs``

       Number of event timer adapters to be used. Each adapter is used in
       round robin manner by the producer cores.

* ``--deq_tmo_nsec``

       Global dequeue timeout for all the event ports if the provided dequeue
       timeout is out of the supported range of event device it will be
       adjusted to the highest/lowest supported dequeue timeout supported.

* ``--crypto_adptr_mode``

        Set crypto adapter mode. Use 0 for OP_NEW (default) and 1 for
        OP_FORWARD mode.

* ``--dma_adptr_mode``

        Set dma adapter mode. DMA adapter supports only OP_FORWARD mode.

* ``--crypto_op_type``

        Set crypto operation type. Use 0 for symmetric crypto ops (default)
        and 1 for asymmetric crypto ops.

* ``--crypto_cipher_alg``

        Cipher algorithm to be used. Default algorithm is NULL.

* ``--crypto_cipher_key``

        Key for the cipher algorithm selected.

* ``--crypto_cipher_iv_sz``

        IV size for the cipher algorithm

* ``--mbuf_sz``

       Set packet mbuf size. Can be used to configure Jumbo Frames. Only
       applicable for `pipeline_atq` and `pipeline_queue` tests.

* ``--max_pkt_sz``

       Set max packet mbuf size. Can be used to configure Rx/Tx scatter gather.
       Only applicable for `pipeline_atq` and `pipeline_queue` tests.

* ``--prod_enq_burst_sz``

       Set producer enqueue burst size. Can be used to configure the number of
       events the producer(s) will enqueue as a burst to the event device.
       Only applicable for `perf_queue` and `perf_atq` test in combination with
       CPU (default) or crypto device (``--prod_type_cryptodev``) producers.

* ``--nb_eth_queues``

       Configure multiple Rx queues per each ethernet port.
       Only applicable for `pipeline_atq` and `pipeline_queue` tests.

* ``--enable_vector``

       Enable event vector for Rx/Tx/crypto adapters.
       Only applicable for `pipeline_*` and `perf_*` tests.

* ``--vector_size``

       Vector size to configure for the Rx/crypto adapter.
       Only applicable for `pipeline_*` and `perf_*` tests.

* ``--vector_tmo_ns``

       Vector timeout nanoseconds to be configured for the Rx/crypto adapter.
       Only applicable for `pipeline_*` and `perf_*` tests.

* ``--per_port_pool``

       Configure unique mempool per ethernet device, the size of each pool
       is equal to `pool_sz`.
       Only applicable for `pipeline_atq` and `pipeline_queue` tests.

* ``--tx_first``

       Transmit given number of packets across all the ethernet device that
       are enabled in the test.
       Only applicable for `pipeline_atq` and `pipeline_queue` tests.

* ``--tx_pkt_sz``

       Packet size to use for `--tx_first`.
       Only applicable for `pipeline_atq` and `pipeline_queue` tests.

* ``--preschedule``

       Enable pre-scheduling of events.
       0 - Disable pre-scheduling.
       1 - Enable pre-scheduling.
       2 - Enable pre-schedule with adaptive mode (Default).


Eventdev Tests
--------------

ORDER_QUEUE Test
~~~~~~~~~~~~~~~~

This is a functional test case that aims at testing the following:

#. Verify the ingress order maintenance.
#. Verify the exclusive(atomic) access to given atomic flow per eventdev port.

.. _table_eventdev_order_queue_test:

.. table:: Order queue test eventdev configuration.

   +---+--------------+----------------+------------------------+
   | # | Items        | Value          | Comments               |
   |   |              |                |                        |
   +===+==============+================+========================+
   | 1 | nb_queues    | 2              | q0(ordered), q1(atomic)|
   |   |              |                |                        |
   +---+--------------+----------------+------------------------+
   | 2 | nb_producers | 1              |                        |
   |   |              |                |                        |
   +---+--------------+----------------+------------------------+
   | 3 | nb_workers   | >= 1           |                        |
   |   |              |                |                        |
   +---+--------------+----------------+------------------------+
   | 4 | nb_ports     | nb_workers +   | Workers use port 0 to  |
   |   |              | 1              | port n-1. Producer uses|
   |   |              |                | port n                 |
   +---+--------------+----------------+------------------------+

.. _figure_eventdev_order_queue_test:

.. figure:: img/eventdev_order_queue_test.*

   order queue test operation.

The order queue test configures the eventdev with two queues and an event
producer to inject the events to q0(ordered) queue. Both q0(ordered) and
q1(atomic) are linked to all the workers.

The event producer maintains a sequence number per flow and injects the events
to the ordered queue. The worker receives the events from ordered queue and
forwards to atomic queue. Since the events from an ordered queue can be
processed in parallel on the different workers, the ingress order of events
might have changed on the downstream atomic queue enqueue. On enqueue to the
atomic queue, the eventdev PMD reorders the event to the original
ingress order(i.e producer ingress order).

When the event is dequeued from the atomic queue by the worker, this test
verifies the expected sequence number of associated event per flow by comparing
the free running expected sequence number per flow.

Application options
^^^^^^^^^^^^^^^^^^^

Supported application command line options are following::

   --verbose
   --dev
   --test
   --socket_id
   --pool_sz
   --plcores
   --wlcores
   --nb_flows
   --nb_pkts
   --worker_deq_depth
   --deq_tmo_nsec

Example
^^^^^^^

Example command to run order queue test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-4 -S 4 --vdev=event_sw0 -- \
                --test=order_queue --plcores 1 --wlcores 2,3


ORDER_ATOMIC Test
~~~~~~~~~~~~~~~~~

This is a functional test similar to the ORDER_QUEUE test,
but differs in two critical ways:

#. Both queues (q0 and q1) are atomic.
   This makes it compatible with the distributed software event device (dsw).
#. Atomicity is verified using spinlocks
   for each combination of flow id and queue id.

.. _table_eventdev_atomic_queue_test:

.. table:: Atomic queue test eventdev configuration.

   +---+--------------+----------------+---------------------------+
   | # | Items        | Value          | Comments                  |
   |   |              |                |                           |
   +===+==============+================+===========================+
   | 1 | nb_queues    | 2              | q0 (atomic), q1 (atomic)  |
   |   |              |                |                           |
   +---+--------------+----------------+---------------------------+
   | 2 | nb_producers | 1              |                           |
   |   |              |                |                           |
   +---+--------------+----------------+---------------------------+
   | 3 | nb_workers   | >= 1           |                           |
   |   |              |                |                           |
   +---+--------------+----------------+---------------------------+
   | 4 | nb_ports     | nb_workers + 1 | Workers use port 0 to     |
   |   |              |                | port n-1. Producer uses   |
   |   |              |                | port n.                   |
   +---+--------------+----------------+---------------------------+

.. _figure_eventdev_atomic_queue_test:

.. figure:: img/eventdev_atomic_queue_test.*

   atomic queue test operation.

When an event is dequeued for processing,
a spinlock is acquired for the flow from which the event was dequeued.
Once processing is complete, the lock is released.
The test will fail if an attempt is made to take a lock that is already held.
This indicates that multiple workers attempted to process the same flow
at the same time, thereby violating atomicity.

.. table:: Atomic queue test queue processing tasks.

   +-----------+---------------------------------------------------+
   | Queue ID  | Processing Task                                   |
   |           |                                                   |
   +===========+===================================================+
   | 0         | Update queue ID for event and re-enqueue.         |
   |           |                                                   |
   +-----------+---------------------------------------------------+
   | 1         | Verify sequence number.                           |
   |           |                                                   |
   +-----------+---------------------------------------------------+

Application options
^^^^^^^^^^^^^^^^^^^

Supported application command line options are following::

   --verbose
   --dev
   --test
   --socket_id
   --pool_sz
   --plcores
   --wlcores
   --nb_flows
   --nb_pkts
   --worker_deq_depth
   --deq_tmo_nsec

Example
^^^^^^^

Example command to run with the software event device:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-4 -S 4 --vdev=event_sw0 -- \
                --test=atomic_queue --plcores 1 --wlcores 2,3

Example command to run with the distributed software event device:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-4 --vdev=event_dsw0 -- \
                --test=atomic_queue --plcores 1 --wlcores 2,3,4


ORDER_ATQ Test
~~~~~~~~~~~~~~

This test verifies the same aspects of ``order_queue`` test, the difference is
the number of queues used, this test operates on a single ``all types queue(atq)``
instead of two different queues for ordered and atomic.

.. _table_eventdev_order_atq_test:

.. table:: Order all types queue test eventdev configuration.

   +---+--------------+----------------+------------------------+
   | # | Items        | Value          | Comments               |
   |   |              |                |                        |
   +===+==============+================+========================+
   | 1 | nb_queues    | 1              | q0(all types queue)    |
   |   |              |                |                        |
   +---+--------------+----------------+------------------------+
   | 2 | nb_producers | 1              |                        |
   |   |              |                |                        |
   +---+--------------+----------------+------------------------+
   | 3 | nb_workers   | >= 1           |                        |
   |   |              |                |                        |
   +---+--------------+----------------+------------------------+
   | 4 | nb_ports     | nb_workers +   | Workers use port 0 to  |
   |   |              | 1              | port n-1.Producer uses |
   |   |              |                | port n.                |
   +---+--------------+----------------+------------------------+

.. _figure_eventdev_order_atq_test:

.. figure:: img/eventdev_order_atq_test.*

   order all types queue test operation.

Application options
^^^^^^^^^^^^^^^^^^^

Supported application command line options are following::

   --verbose
   --dev
   --test
   --socket_id
   --pool_sz
   --plcores
   --wlcores
   --nb_flows
   --nb_pkts
   --worker_deq_depth
   --deq_tmo_nsec

Example
^^^^^^^

Example command to run order ``all types queue`` test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-4 -- \
                        --test=order_atq --plcores 1 --wlcores 2,3


ATOMIC_ATQ Test
~~~~~~~~~~~~~~~

This test verifies the same aspects of ``atomic_queue`` test,
the difference is the number of queues used,
this test operates on a single ``all types queue(atq)``
instead of two different atomic queues.

.. _table_eventdev_atomic_atq_test:

.. table:: Order all types queue test eventdev configuration.

   +---+--------------+----------------+-------------------------+
   | # | Items        | Value          | Comments                |
   |   |              |                |                         |
   +===+==============+================+=========================+
   | 1 | nb_queues    | 1              | q0 (all types queue)    |
   |   |              |                |                         |
   +---+--------------+----------------+-------------------------+
   | 2 | nb_producers | 1              |                         |
   |   |              |                |                         |
   +---+--------------+----------------+-------------------------+
   | 3 | nb_workers   | >= 1           |                         |
   |   |              |                |                         |
   +---+--------------+----------------+-------------------------+
   | 4 | nb_ports     | nb_workers + 1 | Workers use port 0 to   |
   |   |              |                | port n-1. Producer uses |
   |   |              |                | port n.                 |
   +---+--------------+----------------+-------------------------+

.. _figure_eventdev_atomic_atq_test:

.. figure:: img/eventdev_atomic_atq_test.*

   atomic all types queue test operation.

Application options
^^^^^^^^^^^^^^^^^^^

Supported application command line options are following::

   --verbose
   --dev
   --test
   --socket_id
   --pool_sz
   --plcores
   --wlcores
   --nb_flows
   --nb_pkts
   --worker_deq_depth
   --deq_tmo_nsec

Example
^^^^^^^

Example command to run order ``all types queue`` test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-4 -- \
                        --test=atomic_atq --plcores 1 --wlcores 2,3


PERF_QUEUE Test
~~~~~~~~~~~~~~~

This is a performance test case that aims at testing the following:

#. Measure the number of events can be processed in a second.
#. Measure the latency to forward an event.

.. _table_eventdev_perf_queue_test:

.. table:: Perf queue test eventdev configuration.

   +---+--------------+----------------+-----------------------------------------+
   | # | Items        | Value          | Comments                                |
   |   |              |                |                                         |
   +===+==============+================+=========================================+
   | 1 | nb_queues    | nb_producers * | Queues will be configured based on the  |
   |   |              | nb_stages      | user requested sched type list(--stlist)|
   +---+--------------+----------------+-----------------------------------------+
   | 2 | nb_producers | >= 1           | Selected through --plcores command line |
   |   |              |                | argument.                               |
   +---+--------------+----------------+-----------------------------------------+
   | 3 | nb_workers   | >= 1           | Selected through --wlcores command line |
   |   |              |                | argument                                |
   +---+--------------+----------------+-----------------------------------------+
   | 4 | nb_ports     | nb_workers +   | Workers use port 0 to port n-1.         |
   |   |              | nb_producers   | Producers use port n to port p          |
   +---+--------------+----------------+-----------------------------------------+

.. _figure_eventdev_perf_queue_test:

.. figure:: img/eventdev_perf_queue_test.*

   perf queue test operation.

The perf queue test configures the eventdev with Q queues and P ports, where
Q and P is a function of the number of workers, the number of producers and
number of stages as mentioned in :numref:`table_eventdev_perf_queue_test`.

The user can choose the number of workers, the number of producers and number of
stages through the ``--wlcores``, ``--plcores`` and the ``--stlist`` application
command line arguments respectively.

The producer(s) injects the events to eventdev based on the first stage sched type
list requested by the user through ``--stlist`` command line argument. It can
inject a burst of events using ``--prod_enq_burst_sz`` command line argument.

Based on the number of stages to process(selected through ``--stlist``),
The application forwards the event to next upstream queue and terminates when it
reaches the last stage in the pipeline. On event termination, application
increments the number events processed and print periodically in one second
to get the number of events processed in one second.

When ``--fwd_latency`` command line option selected, the application inserts
the timestamp in the event on the first stage and then on termination, it
updates the number of cycles to forward a packet. The application uses this
value to compute the average latency to a forward packet.

When ``--prod_type_ethdev`` command line option is selected, the application
uses the probed ethernet devices as producers by configuring them as Rx
adapters instead of using synthetic producers.

Application options
^^^^^^^^^^^^^^^^^^^

Supported application command line options are following::

        --verbose
        --dev
        --test
        --socket_id
        --pool_sz
        --plcores
        --wlcores
        --stlist
        --nb_flows
        --nb_pkts
        --worker_deq_depth
        --fwd_latency
        --queue_priority
        --prod_type_ethdev
        --prod_type_timerdev_burst
        --prod_type_timerdev
        --prod_type_cryptodev
        --prod_type_dmadev
        --prod_type_vector
        --prod_enq_burst_sz
        --timer_tick_nsec
        --max_tmo_nsec
        --expiry_nsec
        --nb_timers
        --nb_timer_adptrs
        --deq_tmo_nsec
        --crypto_adptr_mode
        --dma_adptr_mode

Example
^^^^^^^

Example command to run perf queue test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-3 -S 1 --vdev=event_sw0 -- \
        --test=perf_queue --plcores=2 --wlcore=3 --stlist=p --nb_pkts=0

Example command to run perf queue test with producer enqueuing a burst of events:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-3 -S 1 --vdev=event_sw0 -- \
        --test=perf_queue --plcores=2 --wlcore=3 --stlist=p --nb_pkts=0 \
        --prod_enq_burst_sz=32

Example command to run perf queue test with ethernet ports:

.. code-block:: console

   sudo build/app/dpdk-test-eventdev -l 0-3 -S 1 --vdev=event_sw0 -- \
        --test=perf_queue --plcores=2 --wlcore=3 --stlist=p --prod_type_ethdev

Example command to run perf queue test with event timer adapter:

.. code-block:: console

   sudo  <build_dir>/app/dpdk-test-eventdev -l 0,4-15 \
                -- --wlcores 4 --plcores 12 --test perf_queue --stlist=a \
                --prod_type_timerdev --fwd_latency

Example command to run perf queue test with event DMA adapter:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-4 -S 1 \
               -- --test=perf_queue --plcores= 2 --wlcore=3 --stlist=a \
               --prod_type_dmadev --dma_adptr_mode=1

PERF_ATQ Test
~~~~~~~~~~~~~~~

This is a performance test case that aims at testing the following with
``all types queue`` eventdev scheme.

#. Measure the number of events can be processed in a second.
#. Measure the latency to forward an event.

.. _table_eventdev_perf_atq_test:

.. table:: Perf all types queue test eventdev configuration.

   +---+--------------+----------------+-----------------------------------------+
   | # | Items        | Value          | Comments                                |
   |   |              |                |                                         |
   +===+==============+================+=========================================+
   | 1 | nb_queues    | nb_producers   | Queues will be configured based on the  |
   |   |              |                | user requested sched type list(--stlist)|
   +---+--------------+----------------+-----------------------------------------+
   | 2 | nb_producers | >= 1           | Selected through --plcores command line |
   |   |              |                | argument.                               |
   +---+--------------+----------------+-----------------------------------------+
   | 3 | nb_workers   | >= 1           | Selected through --wlcores command line |
   |   |              |                | argument                                |
   +---+--------------+----------------+-----------------------------------------+
   | 4 | nb_ports     | nb_workers +   | Workers use port 0 to port n-1.         |
   |   |              | nb_producers   | Producers use port n to port p          |
   +---+--------------+----------------+-----------------------------------------+

.. _figure_eventdev_perf_atq_test:

.. figure:: img/eventdev_perf_atq_test.*

   perf all types queue test operation.


The ``all types queues(atq)`` perf test configures the eventdev with Q queues
and P ports, where Q and P is a function of the number of workers and number of
producers as mentioned in :numref:`table_eventdev_perf_atq_test`.


The atq queue test functions as same as ``perf_queue`` test. The difference
is, It uses, ``all type queue scheme`` instead of separate queues for each
stage and thus reduces the number of queues required to realize the use case
and enables flow pinning as the event does not move to the next queue.


Application options
^^^^^^^^^^^^^^^^^^^

Supported application command line options are following::

        --verbose
        --dev
        --test
        --socket_id
        --pool_sz
        --plcores
        --wlcores
        --stlist
        --nb_flows
        --nb_pkts
        --worker_deq_depth
        --fwd_latency
        --prod_type_ethdev
        --prod_type_timerdev_burst
        --prod_type_timerdev
        --prod_type_cryptodev
        --prod_type_dmadev
        --prod_type_vector
        --timer_tick_nsec
        --max_tmo_nsec
        --expiry_nsec
        --nb_timers
        --nb_timer_adptrs
        --deq_tmo_nsec
        --crypto_adptr_mode
        --dma_adptr_mode

Example
^^^^^^^

Example command to run perf ``all types queue`` test:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-3 -- \
                --test=perf_atq --plcores=2 --wlcore=3 --stlist=p --nb_pkts=0

Example command to run perf ``all types queue`` test with event timer adapter:

.. code-block:: console

   sudo  <build_dir>/app/dpdk-test-eventdev -l 0,4-15 \
                -- --wlcores 4 --plcores 12 --test perf_atq --verbose 20 \
                --stlist=a --prod_type_timerdev --fwd_latency

Example command to run perf atq test with event DMA adapter:

.. code-block:: console

   sudo <build_dir>/app/dpdk-test-eventdev -l 0-4 -S 1 \
               -- --test=perf_atq --plcores= 2 --wlcore=3 --stlist=a \
               --prod_type_dmadev --dma_adptr_mode=1

PIPELINE_QUEUE Test
~~~~~~~~~~~~~~~~~~~

This is a pipeline test case that aims at testing the following:

#. Measure the end-to-end performance of an event dev with a ethernet dev.
#. Maintain packet ordering from Rx to Tx.

.. _table_eventdev_pipeline_queue_test:

.. table:: Pipeline queue test eventdev configuration.

   +---+--------------+----------------+-----------------------------------------+
   | # | Items        | Value          | Comments                                |
   |   |              |                |                                         |
   +===+==============+================+=========================================+
   | 1 | nb_queues    | (nb_producers  | Queues will be configured based on the  |
   |   |              | * nb_stages) + | user requested sched type list(--stlist)|
   |   |              | nb_producers   | At the last stage of the schedule list  |
   |   |              |                | the event is enqueued onto per port     |
   |   |              |                | unique queue which is then Transmitted. |
   +---+--------------+----------------+-----------------------------------------+
   | 2 | nb_producers | >= 1           | Producers will be configured based on   |
   |   |              |                | the number of detected ethernet devices.|
   |   |              |                | Each ethdev will be configured as an Rx |
   |   |              |                | adapter.                                |
   +---+--------------+----------------+-----------------------------------------+
   | 3 | nb_workers   | >= 1           | Selected through --wlcores command line |
   |   |              |                | argument                                |
   +---+--------------+----------------+-----------------------------------------+
   | 4 | nb_ports     | nb_workers +   | Workers use port 0 to port n.           |
   |   |              | (nb_produces * | Producers use port n+1 to port n+m,     |
   |   |              | 2)             | depending on the Rx adapter capability. |
   |   |              |                | Consumers use port n+m+1 to port n+o    |
   |   |              |                | depending on the Tx adapter capability. |
   +---+--------------+----------------+-----------------------------------------+

.. _figure_eventdev_pipeline_queue_test_generic:

.. figure:: img/eventdev_pipeline_queue_test_generic.*

.. _figure_eventdev_pipeline_queue_test_internal_port:

.. figure:: img/eventdev_pipeline_queue_test_internal_port.*

   pipeline queue test operation.

The pipeline queue test configures the eventdev with Q queues and P ports,
where Q and P is a function of the number of workers, the number of producers
and number of stages as mentioned in :numref:`table_eventdev_pipeline_queue_test`.

The user can choose the number of workers and number of stages through the
``--wlcores`` and the ``--stlist`` application command line arguments
respectively.

The number of producers depends on the number of ethernet devices detected and
each ethernet device is configured as a event_eth_rx_adapter that acts as a
producer.

The producer(s) injects the events to eventdev based the first stage sched type
list requested by the user through ``--stlist`` the command line argument.

Based on the number of stages to process(selected through ``--stlist``),
The application forwards the event to next upstream queue and when it reaches
the last stage in the pipeline if the event type is ``atomic`` it is enqueued
onto ethdev Tx queue else to maintain ordering the event type is set to
``atomic`` and enqueued onto the last stage queue.

If the ethdev and eventdev pair have ``RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT``
capability then the worker cores enqueue the packets to the eventdev directly
using ``rte_event_eth_tx_adapter_enqueue`` else the worker cores enqueue the
packet onto the ``SINGLE_LINK_QUEUE`` that is managed by the Tx adapter.
The Tx adapter dequeues the packet and transmits it.

On packet Tx, application increments the number events processed and print
periodically in one second to get the number of events processed in one
second.


Application options
^^^^^^^^^^^^^^^^^^^

Supported application command line options are following::

        --verbose
        --dev
        --test
        --socket_id
        --pool_sz
        --wlcores
        --stlist
        --worker_deq_depth
        --prod_type_ethdev
        --deq_tmo_nsec
        --nb_eth_queues
        --enable_vector
        --vector_size
        --vector_tmo_ns
        --per_port_pool
        --tx_first
        --tx_pkt_sz


.. Note::

    * The ``--prod_type_ethdev`` is mandatory for running this test.

Example
^^^^^^^

Example command to run pipeline queue test:

.. code-block:: console

    sudo <build_dir>/app/dpdk-test-eventdev -l 0-3 -S 3 --vdev=event_sw0 -- \
        --test=pipeline_queue --wlcore=1 --prod_type_ethdev --stlist=a

Example command to run pipeline atq test with vector events:

.. code-block:: console

    sudo <build_dir>/app/dpdk-test-eventdev -l 0-3 -S 3 --vdev=event_sw0 -- \
        --test=pipeline_queue --wlcore=1 --prod_type_ethdev --stlist=a \
        --enable_vector  --vector_size 512

PIPELINE_ATQ Test
~~~~~~~~~~~~~~~~~~~

This is a pipeline test case that aims at testing the following with
``all types queue`` eventdev scheme.

#. Measure the end-to-end performance of an event dev with a ethernet dev.
#. Maintain packet ordering from Rx to Tx.

.. _table_eventdev_pipeline_atq_test:

.. table:: Pipeline atq test eventdev configuration.

   +---+--------------+----------------+-----------------------------------------+
   | # | Items        | Value          | Comments                                |
   |   |              |                |                                         |
   +===+==============+================+=========================================+
   | 1 | nb_queues    | nb_producers + | Queues will be configured based on the  |
   |   |              | x              | user requested sched type list(--stlist)|
   |   |              |                | where x = nb_producers in generic       |
   |   |              |                | pipeline and 0 if all the ethdev        |
   |   |              |                | being used have Internal port capability|
   +---+--------------+----------------+-----------------------------------------+
   | 2 | nb_producers | >= 1           | Producers will be configured based on   |
   |   |              |                | the number of detected ethernet devices.|
   |   |              |                | Each ethdev will be configured as an Rx |
   |   |              |                | adapter.                                |
   +---+--------------+----------------+-----------------------------------------+
   | 3 | nb_workers   | >= 1           | Selected through --wlcores command line |
   |   |              |                | argument                                |
   +---+--------------+----------------+-----------------------------------------+
   | 4 | nb_ports     | nb_workers +   | Workers use port 0 to port n.           |
   |   |              | nb_producers + | Producers use port n+1 to port n+m,     |
   |   |              | x              | depending on the Rx adapter capability. |
   |   |              |                | x = nb_producers in generic pipeline and|
   |   |              |                | 0 if all the ethdev being used have     |
   |   |              |                | Internal port capability.               |
   |   |              |                | Consumers may use port n+m+1 to port n+o|
   |   |              |                | depending on the Tx adapter capability. |
   +---+--------------+----------------+-----------------------------------------+

.. _figure_eventdev_pipeline_atq_test_generic:

.. figure:: img/eventdev_pipeline_atq_test_generic.*

.. _figure_eventdev_pipeline_atq_test_internal_port:

.. figure:: img/eventdev_pipeline_atq_test_internal_port.*

   pipeline atq test operation.

The pipeline atq test configures the eventdev with Q queues and P ports,
where Q and P is a function of the number of workers, the number of producers
and number of stages as mentioned in :numref:`table_eventdev_pipeline_atq_test`.

The atq queue test functions as same as ``pipeline_queue`` test. The difference
is, It uses, ``all type queue scheme`` instead of separate queues for each
stage and thus reduces the number of queues required to realize the use case.


Application options
^^^^^^^^^^^^^^^^^^^

Supported application command line options are following::

        --verbose
        --dev
        --test
        --socket_id
        --pool_sz
        --wlcores
        --stlist
        --worker_deq_depth
        --prod_type_ethdev
        --deq_tmo_nsec
        --nb_eth_queues
        --enable_vector
        --vector_size
        --vector_tmo_ns
        --per_port_pool
        --tx_first
        --tx_pkt_sz


.. Note::

    * The ``--prod_type_ethdev`` is mandatory for running this test.

Example
^^^^^^^

Example command to run pipeline atq test:

.. code-block:: console

    sudo <build_dir>/app/dpdk-test-eventdev -l 0-3 -- \
        --test=pipeline_atq --wlcore=1 --prod_type_ethdev --stlist=a

Example command to run pipeline atq test with vector events:

.. code-block:: console

    sudo <build_dir>/app/dpdk-test-eventdev -l 0-3 -- \
        --test=pipeline_atq --wlcore=1 --prod_type_ethdev --stlist=a \
        --enable_vector  --vector_size 512

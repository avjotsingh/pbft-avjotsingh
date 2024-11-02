- add timeout after receiving majority of promise messages
- synchronize acceptor if accept val is already committed (reset acceptor's acceptnum and acceptval)
- randomize delays after prepare phase failure
- check if accept num and accept val have already been committed and ignore those values if committed
- revisit reset consensus deadline
- compare block proposal number while syncing up
- is last committed really needed in accept req?
- if server is down, catch exceptions on client requests


States -> IDLE, PREPARE, PROPOSE, PROMISED, ACCEPTED, COMMIT, CATCH_UP

Transitions

On transfer message
- IDLE->PREPARE

On prepare message
- IDLE->PROMISED
- IDLE->CATCHUP
- PREPARE->PROMISED
- PREPARE->CATCHUP

On accept message
- s


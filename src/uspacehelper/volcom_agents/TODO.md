## FLOW

-   Employee broadcast the resouceses
-   Employer disting to that boradcast port
-   Employer send his ip, resouces he wants to get by worker
-   Employee receive that config
-   Emplyee allocate resouces, ack request with current resouces allocation
-   Employer send script and file sending details [`need to clarify wethere direct connection or download the send to the node`] [`Research`]
-   Empoyee recive files untill buffer full (80%), send empyer to stop [`Research`]
-   Empyer revice the message and stop
-   Empyee send the result for each data point [`run parallel`]
-   Empyee asked fro more when buffer is low (20%) [`Research`]
-   Empyer listing to that message and send data point streaming again
-   Emplyee can connect with multiple empyers so he continuosly send its resulses avalivality until a thehold [`Research`]

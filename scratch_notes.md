client side:
- [v] for every file
    - [v'] while not all packets have been delivered
        - [v'] send packets
    - [v] read checksum packet
    - [] if not timeout
        - [] compute hash and compare server's hash
        - [] send confirmation to server
        - [] read server response
        - [] if not timeout
            - [] return (copy next file)
        - [] while timeout and retries available:
            - [] resend
            - [] read server response
        - [] if no retries available
            - [] declare network failure
    - [] while timeout and retries available:
        - [v] resend
        - [v] read server response
    - [] if no retries available
        - [] declare network failure


server side:
- [v] while 1
    - [v] keep reading data packets until full file
    - [v] compute hash and send to server
    - [] read confirmation
        - [] if not timeout
            - [] rename/delete
            - [] notify client
        - [] while timeout and retries available
            - [] resend
            - [] read confirmation

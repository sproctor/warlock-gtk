// var match_queue = new Queue()
var match_callbacks = new Array()

function match (str, exc)
{
        function match_handler (s)
        {
                if (s.indexOf (str) != -1) {
                        //if (exc) match_queue.put (exc)
                        //else match_queue.put (False)
                }
        }
        match_callbacks.push (match_handler)
        addCallback (match_handler)
}

function matchWait ()
{
        //var exc = match_queue.pop ()
        for (x in match_callbacks) {
                removeCallback (x)
        }
        match_callbacks = new Array()
        //match_queue = new Queue()
        if (exc) throw exc
}

function waitFor (str)
{
        var wait_queue = new Queue();

        debug ("waiting for string: " + str + "\n");

        function wait_handler(s, str, waitq) {
                debug ("Running wait handler on line: " + s + "\n")
                debug ("matching string: " + str + "\n")
                if (s.indexOf (str) != -1) {
                        debug ("strings match\n")
                        waitq.push (true)
			debug ("done with successful match\n")
			return false;
                } else {
			debug ("strings didn't match\n")
		}
        }
	addCallback(wait_handler, str, wait_queue);
        wait_queue.pop ();
}

#!./run

/*
* this is stupid, and only really for testing.
*/

class CGI
{
    constructor()
    {
        this.m_params = {}
        this.m_pinfo = []
        pinf = System.getenv("PATH_INFO");
        if(pinf != null)
        {
            this.m_pinfo = pinf.split("/").filter((s) => s.length > 0)
        }
        STDERR.write("pinf=%p, m_pinfo = %p\n".format(pinf, this.m_pinfo))
    }

    pathIsIndex()
    {
        return ((this.m_pinfo.length == 0) || this.pathIs("index"));
    }

    pathIs(p)
    {
        return (this.m_pinfo[0] == p);
    }

    beginHeaders(code)
    {
        //STDOUT.write("HTTP/1.1 %d OK\r\n".format(code))
        STDOUT.write("Status: %d\r\n".format(code))
        
    }

    endHeaders()
    {
        STDOUT.write("\r\n")
    }

    sendHeader(k, v)
    {
        STDOUT.write("%s: %s\r\n".format(k, v))
    }
    
}


class HTMLGen
{
    constructor(out)
    {
        this.out = out
    }

    put(s)
    {
        this.out.write(s)
    }

    tag(name, attribs, fn)
    {
        if(attribs.length == 0)
        {
            this.put("<%s>".format(name))
        }
        else
        {
            var vi = 0;
            this.put("<%s ".format(name))
            for(var k in attribs)
            {
                vi++;
                this.put("%s=\"%s\"".format(k, attribs[k]))
                if((vi+1) < attribs.length)
                {
                    this.put(" ")
                }
            }
            this.put(">")
        }
        if(fn.class.name == "Function")
        {
            fn(this)
        }
        else
        {
            this.put(fn)
        }
        this.put("</%s>".format(name))
    }
    
}

class App
{
    constructor()
    {
        this.cgi = new CGI()
    }

    sendPage(ctype, code)
    {
        this.cgi.beginHeaders(code)
        this.cgi.sendHeader("Content-Type", ctype)
        this.cgi.endHeaders()
    }

    sendError(code)
    {
        this.sendPage("text/html", code)
        STDOUT.write("no such page.")
    }

    sendIndex()
    {
        this.sendPage("text/html", 200)
        gh = new HTMLGen(STDOUT)
        gh.tag("html", {}, ()=>
        {
            gh.tag("head", {}, ()=>{})
            gh.tag("body", {}, ()=>
            {
                gh.tag("h1", {}, "files: ")
                gh.tag("ul", {}, ()=>
                {
                    for(var itm in Dir.read("."))
                    {
                        if((itm == ".") || (itm == ".."))
                        {
                            continue;
                        }
                        url = ("web.cgi/v?f=%s".format(itm));
                        gh.tag("li", {}, ()=>
                        {
                            gh.tag("a", {"href": url}, itm)
                        });
                        gh.put("\n");
                    }
                })
            })
        })
    }

    main()
    {
        if(this.cgi.pathIsIndex())
        {
            this.sendIndex()
        }
        else
        {
            this.sendError(404);
        }
    }
}

App().main()

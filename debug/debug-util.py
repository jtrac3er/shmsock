
"""
Nuetzliche commands zum debuggen, wie das anzeigen von linked lists usw

"""

import argparse
import traceback

class glob:
	var_ctr = 0

def argparse_error(self, msg):
	pass

argparse.error = argparse_error

def value_to_uint64(value):
	return value & ((1 << 64) -1)


def bitslice(value, fr, to):
	return (((1 << fr) - 1) & value) >> to

def gdbReturn(value):
	gdb.execute("set $r=%i" % value)


def gdbGetReturn(value):
	return gdb.parse_and_eval("$r")

def parseArgs(args): 
	return [i for i in args.split(" ") if i != ""]

def executeAndParseVar(cmd):
	var = "$e_%i" % glob.var_ctr
	glob.var_ctr += 1
	gdb.execute("set %s=%s" % (var, cmd))
	return var
	
def executeAndParse(cmd):
	gdb.execute("set $e=%s" % cmd)
	return gdb.parse_and_eval("$e")
	
def printExpression(expr):
	gdb.execute("p %s" % expr)


# erzeugt eine Funktion, welche die Argumente kriegt
def createCustomCallback(func, *args, **kwargs):
	
	def retFunc(bp):
		return func(bp, *args, **kwargs)
	
	return retFunc
	
	

class CustomBreakpoint(gdb.Breakpoint):

	def __init__(self, func, *args, **kwargs):
		self.func = func
		super(CustomBreakpoint, self).__init__(*args, **kwargs)
		
	def stop(self):
		self.func(self)
				
				
class setEnvironment(gdb.Command):

	
	""" set environment for debugging either server or client \
	"""

	def __init__(self):
		super(setEnvironment, self).__init__(
			"setEnvironment", gdb.COMMAND_USER
		)
	
	def invoke(self, args, from_tty):
		try:
			parser = argparse.ArgumentParser()
			
			parser.add_argument("-m", "--mode", choices=["server", "client"], dest="mode", required=True)
			parser.add_argument("-c", "--connection-id", nargs=1, type=str, dest="cdir", default="./")
			#parser.add_argument("-i", "-iface-start", nargs="?", type=int, dest="iface_start", default=0)
			parser.add_argument("-io", "-own-addr", nargs=1, type=int, dest="own_addr", required=True)
			parser.add_argument("-ir", "-remote-addr", nargs=1, type=int, dest="rem_addr", required=True)
			
			try:
				args = parser.parse_args(parseArgs(args))
				args.cdir = args.cdir[0]
				args.own_addr = args.own_addr[0]
				args.rem_addr = args.rem_addr[0]
			except SystemExit as e:
				return
				
			iface_start = 0
			iface_count = 1
					
			#print(args.cdir)	#liste, weiso???
				
			# auf connection id setzen. Der ordner muss existieren
			debuggingBaseDir = "/home/ubuntu/shmsockv2/debug/conndir/"
			cdir = debuggingBaseDir + args.cdir + "/"
			gdb.execute("shell mkdir -p %s" % cdir)
			gdb.execute("shell touch %s/iface.%i" % (cdir, iface_start))
			gdb.execute("set environment %s=%s" % ("SHMSOCK_DIR", cdir))	# fuer moment
			
			gdb.execute("set environment %s=%s" % ("SHMSOCK_IFACE_START", iface_start))	# fuer moment
			gdb.execute("set environment %s=%s" % ("SHMSOCK_IFACE_COUNT", iface_count))	# fuer moment
			gdb.execute("set environment %s=%s" % ("SHMSOCK_OWN_ADDRESS_0", args.own_addr))
			gdb.execute("set environment %s=%s" % ("SHMSOCK_REMOTE_ADDRESS_0", args.rem_addr))
			
			if args.mode == "server":
				gdb.execute("set environment %s=%s" % ("LD_PRELOAD", 
						"/home/ubuntu/shmsockv2/lib/libshmsock-server.so"))
			else:
				gdb.execute("set environment %s=%s" % ("LD_PRELOAD", 
						"/home/ubuntu/shmsockv2/lib/libshmsock-client.so"))
			
				
		except Exception:
			print(traceback.format_exc())
			
		

setEnvironment()


class followConnections(gdb.Command):

	""" follows through connection lifecyle \
	"""

	def __init__(self):
		super(followConnections, self).__init__(
			"followConnections", gdb.COMMAND_USER
		)
		
	def invoke(self, args, from_tty):
		try:
		
			parser = argparse.ArgumentParser()
			
			# interface
			parser.add_argument("-i", type=int, dest="ifaceNo", default=0)
			
			# welche events man anschauen soll
			parser.add_argument("-s", nargs="+", dest="states", choices=["*", "create", "open", "close"], 
					type=str, default=[None], required=True)	
			
			try:
				args = parser.parse_args(parseArgs(args))
				if "*"  in args.states:
					args.states = ["create", "open", "close"]
			except SystemExit as e:
				return
				
			
			# types
			ts_interface = gdb.lookup_type("struct interface")
			tp_interface = ts_interface.pointer()
			ts_connection = gdb.lookup_type("struct connection")
			tp_connection = ts_connection.pointer()
			
			# global variables
			s_interfaces = gdb.parse_and_eval("interfaces")	
			if s_interfaces.address == 0x0:
				print("Programm muss laufen um breakpoint zu setzen")
				
			#print(s_interfaces[0].type)	# genial, man kann wirklich so indexieren
			
			def cbStateChange(bp):
				print("State der Connection wurde geaendert")
				return True
			
			def cbCreation(bp, addr_listenList):
				try:
					if addr_listenList == gdb.selected_frame().read_register("rdi"):
						
						# addresse herausfinden
						pConn = gdb.Value(gdb.selected_frame().read_register("rsi"))
						pConn = pConn.cast(tp_connection)
							
						addr_pConn_state = pConn.dereference()["connState"].address
						
						# einen watchpoint setzen
						print("Watchpoint gesetzt bei %x" % addr_pConn_state)
						bp_state_change = CustomBreakpoint(
							createCustomCallback(cbStateChange),
							"*" + str(addr_pConn_state),
							gdb.BP_WATCHPOINT,
							gdb.WP_WRITE,
						)
						
						print("Neue Connection wurde erstellt")
						return True
						
					else:
						return True
						
				except Exception: print(traceback.format_exc())
					
					
				
			p_iface = s_interfaces[args.ifaceNo]
			s_connectionList = p_iface.dereference()["connectionList"]
			
			# also creation eines neuen Interfaces aufnehmen
			bp_conn_creation = CustomBreakpoint(
				createCustomCallback(cbCreation, s_connectionList.address),
				"llist_insert_first",
			)
				
		except Exception: print(traceback.format_exc())	
		
		
	
followConnections()
	

class printConnections(gdb.Command):

	""" prints all connections with some extra information \
	"""

	def __init__(self):
		super(printConnections, self).__init__(
			"printConnections", gdb.COMMAND_USER
		)

	def invoke(self, args, from_tty):
		try:
		
			parser = argparse.ArgumentParser()

			# gibt an, welches interface man nehmen soll
			parser.add_argument("-i", nargs=1, type=int, dest="ifaceNo", default=[0])	
					
			# gibt an, welche connection in welchem Zustand man anzeigen soll
			parser.add_argument("-s", nargs=1, dest="state", choices=["create", "open", "close"], type=str, default=[None])	
			connStates = {"create" : 0, "open" : 1, "close" : 2}
			
			try:
				args = parser.parse_args(parseArgs(args))
				args.ifaceNo = args.ifaceNo[0]
				args.state = args.state[0]
			except SystemExit as e:
				return
			
			# liste durchgehen
			var_interface = executeAndParseVar("interfaces[%d]" % args.ifaceNo)
			#printExpression(var_interface)
			
			if executeAndParse(var_interface) == 0x0:
				print("Interface existiert nicht")
				return
			
			var_list_head = "%s->connectionList" % var_interface
			
			#print(var_list_head)	# direkt {list = 0x7f81e79670b8, malloc_ctx = 0x7f81e7967018} also das struct
									# solange es geht mit expressions arbeiten, nicht auswerten
			
			var_list_size = executeAndParse("llist_len(&%s)" % var_list_head)
			print("Anzahl Connections: %i" % var_list_size)
			
			for i in range(var_list_size):
				pConn = executeAndParseVar("(struct connection*)llist_get_n(&%s, %i)" %
					(var_list_head, i))
				conn_state = executeAndParseVar("%s->connState" % 
					pConn)
					
				if args.state and conn_state != connStates[args.state]:
					# falscher Zustand, weitermachen
					continue
					
				recv_buffer = executeAndParseVar("&%s->recvBuffer" % 
					pConn)
				send_buffer = executeAndParseVar("&%s->sendBuffer" % 
					pConn)
				
				recv_len = executeAndParse("llist_len(%s)" %
					recv_buffer)
					
				send_len = executeAndParse("llist_len(%s)" %
					send_buffer)
					
				# kurzes summary printen
				print("\nSummary Connection: recv_len=%i, send_len=%i, pConn:" %
					(recv_len, send_len))
				printExpression("*%s" % pConn)			

		except Exception:
			print(traceback.format_exc())
		

printConnections()












// Define explicitly supplied, independently verified function entry addresses.
// Intended for a fast -noanalysis import; never guesses a containing function.
// @category HaloMCCVR.RE
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
public class DefineEvidenceFunctions extends GhidraScript {
    @Override public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            Address address = toAddr(arg);
            if (getFunctionAt(address) == null) {
                disassemble(address);
                if (createFunction(address, null) == null)
                    throw new IllegalStateException("Cannot define " + arg);
            }
            println("EXPLICIT_ENTRY " + address);
        }
    }
}

// Bounded instruction evidence around already identified kit/retail functions.
// Arguments: <hex RVA>:<instruction count> [...]. Use -readOnly -noanalysis.
// @category HaloMCCVR.RE
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class DumpInstructionRange extends GhidraScript {
    public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            String[] parts=arg.split(":");
            if(parts.length!=2) throw new IllegalArgumentException("RVA:instruction-count");
            int count=Integer.parseInt(parts[1]);
            if(count<1 || count>300) throw new IllegalArgumentException("count must be 1..300");
            Address start=currentProgram.getImageBase().add(Long.parseUnsignedLong(parts[0],16));
            Instruction ins=getInstructionAt(start);
            if(ins==null) ins=getInstructionContaining(start);
            if(ins==null) throw new IllegalArgumentException("No decoded instruction at "+start);
            println("PROGRAM "+currentProgram.getName()+" RANGE "+arg);
            for(int i=0;i<count && ins!=null;++i) {
                println(ins.getAddress()+" "+ins);
                ins=ins.getNext();
            }
        }
    }
}

// Match a kit-proven field in small retail functions; print instructions for review.
// @category HaloMCCVR.RE
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class FindSmallFieldReaders extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("maxBytes fieldHex [...]");
        long limit = Long.parseLong(args[0]);
        List<Pattern> fields = new ArrayList<>();
        for (int i=1; i<args.length; ++i)
            fields.add(Pattern.compile("0x" + args[i].toLowerCase() + "(?:\\]|,|\\))"));
        int count=0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (f.getBody().getNumAddresses() > limit) continue;
            List<Instruction> code = new ArrayList<>();
            boolean match=false;
            for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                code.add(ins);
                String value=ins.toString().toLowerCase();
                for (Pattern field : fields) match |= field.matcher(value).find();
            }
            if (!match) continue;
            println("CANDIDATE RVA=" + Long.toHexString(f.getEntryPoint().subtract(currentProgram.getImageBase()))
                + " bytes=" + f.getBody().getNumAddresses());
            for (Instruction ins : code) println(ins.getAddress() + " " + ins);
            count++;
        }
        println("CANDIDATES " + count);
    }
}

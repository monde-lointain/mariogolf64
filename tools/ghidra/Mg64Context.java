import java.io.StringWriter;
import java.util.*;
import com.google.gson.Gson;
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.util.LongPropertyMap;

/** Read-only export invoked by the project CLI, never arbitrary agent-provided source. */
public class Mg64Context extends GhidraScript {
    private static String declaration(DataType type, String name) {
        if (name == null) name = "";
        if (type instanceof Array array) return declaration(array.getDataType(), name + "[" + array.getNumElements() + "]");
        if (type instanceof Pointer pointer) {
            DataType element = pointer.getDataType();
            if (element == null) return "void *" + name;
            String target = "*" + name;
            if (element instanceof Array || element instanceof FunctionDefinition) target = "(" + target + ")";
            return declaration(element, target);
        }
        if (type instanceof FunctionDefinition function) {
            List<String> parameters = new ArrayList<>();
            for (ParameterDefinition parameter : function.getArguments()) parameters.add(declaration(parameter.getDataType(), parameter.getName()));
            if (function.hasVarArgs()) parameters.add("...");
            if (parameters.isEmpty()) parameters.add("void");
            return declaration(function.getReturnType(), name + "(" + String.join(", ", parameters) + ")");
        }
        String prefix = type instanceof Structure ? "struct " : type instanceof Union ? "union " : type instanceof ghidra.program.model.data.Enum ? "enum " : "";
        return prefix + type.getName() + " " + name;
    }

    private static String prototype(Function function, Map<DataType, DataType> exported) {
        List<String> parameters = new ArrayList<>();
        for (Parameter parameter : function.getParameters()) parameters.add(declaration(exported.get(parameter.getDataType()), parameter.getName()));
        if (function.hasVarArgs()) parameters.add("...");
        if (parameters.isEmpty()) parameters.add("void");
        return declaration(exported.get(function.getReturnType()), function.getName() + "(" + String.join(", ", parameters) + ")");
    }

    private static void collectType(DataType type, Set<DataType> types) {
        if (type == null || !types.add(type)) return;
        if (type instanceof Pointer pointer) collectType(pointer.getDataType(), types);
        else if (type instanceof Array array) collectType(array.getDataType(), types);
        else if (type instanceof TypeDef alias) collectType(alias.getBaseDataType(), types);
        else if (type instanceof FunctionDefinition function) {
            collectType(function.getReturnType(), types);
            for (ParameterDefinition parameter : function.getArguments()) collectType(parameter.getDataType(), types);
        }
    }

    private static Map<DataType, DataType> exportTypes(Set<DataType> types, StandAloneDataTypeManager manager) throws Exception {
        Map<DataType, DataType> copies = new HashMap<>();
        int tx = manager.startTransaction("Build isolated C export types");
        try {
            for (DataType type : types) copies.put(type, manager.resolve(type, DataTypeConflictHandler.DEFAULT_HANDLER));
            List<DataType> named = new ArrayList<>();
            Iterator<DataType> iterator = manager.getAllDataTypes();
            while (iterator.hasNext()) {
                DataType type = iterator.next();
                if (type instanceof Composite || type instanceof TypeDef || type instanceof ghidra.program.model.data.Enum || type instanceof FunctionDefinition) named.add(type);
            }
            named.sort(Comparator.comparing(DataType::getPathName));
            Set<String> reserved = new HashSet<>(), used = new HashSet<>();
            for (DataType type : named) if (type.getName().matches("[A-Za-z_][A-Za-z0-9_]*")) reserved.add(type.getName());
            for (DataType type : named) {
                String original = type.getName();
                String name = original.replaceAll("[^A-Za-z0-9_]", "_");
                if (name.isEmpty() || Character.isDigit(name.charAt(0))) name = "_" + name;
                if (used.contains(name) || (!name.equals(original) && reserved.contains(name))) {
                    name += "_" + Integer.toUnsignedString(type.getPathName().hashCode(), 16);
                    while (used.contains(name) || reserved.contains(name)) name += "_";
                }
                used.add(name);
                if (!name.equals(original)) type.setName(name);
            }
        } finally { manager.endTransaction(tx, true); }
        return copies;
    }
    private static boolean knownPrototype(Function function) {
        SourceType source = function.getSignatureSource();
        return source == SourceType.IMPORTED || source == SourceType.USER_DEFINED;
    }


    public void run() throws Exception {
        String[] args = getScriptArgs();
        Map<String, Object> result = new LinkedHashMap<>();
        result.put("program", currentProgram.getName());
        result.put("manifest", currentProgram.getOptions("MG64").getString("Manifest", ""));
        result.put("rom_sha1", currentProgram.getOptions("MG64").getString("ROM SHA1", ""));
        result.put("manifest_id", currentProgram.getOptions("MG64").getString("Manifest ID", ""));
        if (args[0].equals("identity")) {
            println("MG64_JSON:" + new Gson().toJson(result));
            return;
        }
        if (args[0].equals("symbols")) {
            List<Map<String, Object>> symbols = new ArrayList<>();
            SymbolIterator iterator = currentProgram.getSymbolTable().getAllSymbols(true);
            while (iterator.hasNext()) {
                Symbol symbol = iterator.next();
                if (symbol.getSource() != SourceType.USER_DEFINED || !symbol.getAddress().isMemoryAddress()) continue;
                if (!symbol.isPrimary()) continue;
                symbols.add(Map.of("name", symbol.getName(), "address", symbol.getAddress().getOffset(),
                    "function", symbol.getSymbolType() == SymbolType.FUNCTION));
            }
            result.put("symbols", symbols);
            println("MG64_JSON:" + new Gson().toJson(result));
            return;
        }
        Address entry = toAddr(args[1]);
        Function function = getFunctionAt(entry);
        if (function == null) throw new IllegalArgumentException("Not a function entry: " + entry);
        result.put("name", function.getName());
        result.put("address", entry.getOffset());
        result.put("signature", function.getSignature().getPrototypeString(false));
        result.put("signature_source", function.getSignatureSource().toString());
        result.put("prototype_known", knownPrototype(function));
        LongPropertyMap romOffsets = currentProgram.getUsrPropertyManager().getLongPropertyMap("mg64_rom_offset");
        if (romOffsets != null && romOffsets.hasProperty(entry)) result.put("rom", romOffsets.getLong(entry));
        DecompInterface decompiler = new DecompInterface();
        try {
            if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException(decompiler.getLastMessage());
            DecompileResults decompiled = decompiler.decompileFunction(function, 90, monitor);
            if (!decompiled.decompileCompleted() || decompiled.getDecompiledFunction() == null) throw new IllegalStateException(decompiled.getErrorMessage());
            result.put("decompiled", decompiled.getDecompiledFunction().getC());
            result.put("warnings", decompiled.getErrorMessage());
            Set<DataType> types = new LinkedHashSet<>();
            collectType(function.getReturnType(), types);
            for (Parameter parameter : function.getParameters()) collectType(parameter.getDataType(), types);
            HighFunction high = decompiled.getHighFunction();
            if (high != null) {
                Iterator<HighSymbol> locals = high.getLocalSymbolMap().getSymbols();
                while (locals.hasNext()) collectType(locals.next().getDataType(), types);
            }
            List<Function> functions = new ArrayList<>();
            functions.add(function);
            List<Map<String, Object>> callees = new ArrayList<>();
            for (Function callee : function.getCalledFunctions(monitor)) {
                collectType(callee.getReturnType(), types);
                for (Parameter parameter : callee.getParameters()) collectType(parameter.getDataType(), types);
                functions.add(callee);
                callees.add(Map.of("name", callee.getName(), "address", callee.getEntryPoint().getOffset(),
                    "signature", callee.getSignature().getPrototypeString(false),
                    "signature_source", callee.getSignatureSource().toString(),
                    "prototype_known", knownPrototype(callee)));
            }
            result.put("callees", callees);
            List<Map<String, Object>> globals = new ArrayList<>();
            Map<String, DataType> globalTypes = new LinkedHashMap<>();
            Set<Address> seen = new HashSet<>();
            List<Map<String, Object>> assembly = new ArrayList<>();
            InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(), true);
            while (instructions.hasNext()) {
                Instruction instruction = instructions.next();
                assembly.add(Map.of("address", instruction.getAddress().getOffset(), "instruction", instruction.toString()));
                for (Reference reference : instruction.getReferencesFrom()) {
                    if (!reference.getReferenceType().isData()) continue;
                    Address target = reference.getToAddress();
                    Data data = currentProgram.getListing().getDefinedDataContaining(target);
                    Address base = data == null ? target : data.getAddress();
                    if (!base.isMemoryAddress() || !seen.add(base)) continue;
                    Symbol symbol = currentProgram.getSymbolTable().getPrimarySymbol(base);
                    if (symbol == null) continue;
                    if (data == null) {
                        globals.add(Map.of("name", symbol.getName(), "address", base.getOffset(), "declaration", "", "typed", false));
                        continue;
                    }
                    DataType type = data.getDataType();
                    if (type instanceof Dynamic dynamic && dynamic.getReplacementBaseType() != null) {
                        DataType element = dynamic.getReplacementBaseType().clone(currentProgram.getDataTypeManager());
                        type = new ArrayDataType(element, data.getLength() / element.getLength(), element.getLength());
                    }
                    collectType(type, types);
                    globalTypes.put(symbol.getName(), type);
                    globals.add(new LinkedHashMap<>(Map.of("name", symbol.getName(), "address", base.getOffset(), "typed", true)));
                }
            }
            result.put("globals", globals);
            result.put("assembly", assembly);
            try (StandAloneDataTypeManager export = new StandAloneDataTypeManager("mg64_context", currentProgram.getDataTypeManager().getDataOrganization())) {
                Map<DataType, DataType> copies = exportTypes(types, export);
                StringWriter header = new StringWriter();
                DataTypeWriter writer = new DataTypeWriter(export, header);
                writer.write(new ArrayList<>(copies.values()), monitor);
                header.append("\n/* Analysis context only; no matching or ABI verdict. */\n");
                for (Function exportedFunction : functions) {
                    // An uncommitted decompiler prototype is not a C `(void)` declaration.
                    if (knownPrototype(exportedFunction)) header.append("extern ").append(prototype(exportedFunction, copies)).append(";\n");
                }
                for (Map<String, Object> global : globals) {
                    if (!Boolean.TRUE.equals(global.get("typed"))) continue;
                    String name = (String) global.get("name");
                    String definition = declaration(copies.get(globalTypes.get(name)), name) + ";";
                    global.put("declaration", definition);
                    header.append("extern ").append(definition).append('\n');
                }
                result.put("signature", prototype(function, copies));
                result.put("context", header.toString());
            }
            println("MG64_JSON:" + new Gson().toJson(result));
        } finally { decompiler.dispose(); }
    }
}

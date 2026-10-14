import java.io.*;
import java.nio.file.*;
import java.util.*;

import com.google.gson.*;
import com.xebyte.core.NamingPolicy;
import com.xebyte.headless.GhidraMCPHeadlessServer;
import ghidra.GhidraApplicationLayout;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.program.util.DefaultLanguageService;
import ghidra.app.util.cparser.C.CParser;
import ghidra.base.project.GhidraProject;
import ghidra.framework.Application;
import ghidra.framework.HeadlessGhidraApplicationConfiguration;
import ghidra.framework.model.*;
import ghidra.program.database.ProgramDB;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.util.LongPropertyMap;
import ghidra.program.model.util.StringPropertyMap;
import ghidra.util.task.TaskMonitor;

/** The original ROM, YAML ranges, extracted boundaries, and source declarations own import. */
public class Mg64Project {
    private static final TaskMonitor MONITOR = TaskMonitor.DUMMY;
    private static final Object CONSUMER = new Object();
    private static final Gson JSON = new GsonBuilder().setPrettyPrinting().create();
    private static final List<Map<String, Object>> reports = new ArrayList<>();
    private static JsonObject manifest;

    private static long number(JsonObject object, String key) { return object.get(key).getAsLong(); }
    private static String text(JsonObject object, String key) { return object.get(key).getAsString(); }
    private static Address address(Program program, long value) {
        return program.getAddressFactory().getDefaultAddressSpace().getAddress(value);
    }
    private static JsonObject segment(String name) {
        for (JsonElement element : manifest.getAsJsonArray("segments")) {
            JsonObject record = element.getAsJsonObject();
            if (text(record, "name").equals(name)) return record;
        }
        throw new IllegalArgumentException("Unknown segment: " + name);
    }

    private static void analyze(Program program) {
        int tx = program.startTransaction("Analyze mapped original code");
        boolean success = false;
        try {
            program.getOptions(Program.ANALYSIS_PROPERTIES).setBoolean("MIPS Constant Reference Analyzer.Mark dual instruction references", true);
            GhidraProject.analyze(program);
            success = true;
        } finally { program.endTransaction(tx, success); }
    }

    private static void mapSegment(Program program, String name) throws Exception {
        Memory memory = program.getMemory();
        for (JsonElement element : segment(name).getAsJsonArray("regions")) {
            JsonObject region = element.getAsJsonObject();
            long length = number(region, "length");
            if (length == 0) continue;
            Address start = address(program, number(region, "address"));
            MemoryBlock block;
            if (number(region, "offset") < 0) {
                block = memory.createUninitializedBlock(text(region, "name"), start, length, false);
            } else {
                try (FileInputStream stream = new FileInputStream(text(manifest, "rom"))) {
                    stream.getChannel().position(number(region, "offset"));
                    block = memory.createInitializedBlock(text(region, "name"), start, stream, length, MONITOR, false);
                }
            }
            block.setRead(true);
            block.setWrite(region.get("write").getAsBoolean());
            block.setExecute(region.get("execute").getAsBoolean());
            block.setSourceName("baserom.z64 / mariogolf64.yaml");
            block.setComment(number(region, "offset") < 0 ? "Uninitialized BSS; not ROM bytes" : String.format("ROM 0x%X", number(region, "offset")));
        }
    }

    private static void hardware(Program program) throws Exception {
        program.getMemory().createUninitializedBlock("interrupt_vectors", address(program, 0x80000000L), 0x400, false);
        Set<Long> pages = new TreeSet<>();
        for (JsonElement element : manifest.getAsJsonArray("symbols")) {
            JsonObject symbol = element.getAsJsonObject();
            if (text(symbol, "source").equals("hardware_regs.txt")) pages.add(number(symbol, "address") & ~0xfffL);
        }
        for (long page : pages) {
            if (program.getMemory().contains(address(program, page))) continue;
            MemoryBlock block = program.getMemory().createUninitializedBlock(String.format("io_%08X", page), address(program, page), 0x1000, false);
            block.setRead(true);
            block.setWrite(true);
            block.setVolatile(true);
        }
    }

    private static void functions(Program program, String name) throws Exception {
        LongPropertyMap offsets = program.getUsrPropertyManager().getLongPropertyMap("mg64_rom_offset");
        if (offsets == null) offsets = program.getUsrPropertyManager().createLongPropertyMap("mg64_rom_offset");
        for (JsonElement element : manifest.getAsJsonObject("functions").getAsJsonArray(name)) {
            JsonObject record = element.getAsJsonObject();
            Address start = address(program, number(record, "address"));
            Address end = start.add(number(record, "size") - 1);
            if (!program.getMemory().contains(end) || !program.getMemory().getBlock(start).isExecute()) {
                throw new IllegalArgumentException("Function outside executable mapping: " + record);
            }
            AddressSet body = new AddressSet(start, end);
            DisassembleCommand disassemble = new DisassembleCommand(start, body, true);
            if (!disassemble.applyTo(program, MONITOR)) {
                throw new IOException("Cannot disassemble " + text(record, "name") + ": " + disassemble.getStatusMsg());
            }
            Function function = program.getFunctionManager().createFunction(text(record, "name"), start, body, SourceType.IMPORTED);
            function.setCallingConvention(program.getCompilerSpec().getDefaultCallingConvention().getName());
            function.setComment(String.format("Boundary from extracted assembly; %s ROM 0x%X, size 0x%X. Not a matching verdict.", name, number(record, "rom"), number(record, "size")));
            offsets.add(start, number(record, "rom"));
        }
    }

    private static boolean symbolApplies(JsonObject symbol, Set<String> included) {
        if (!symbol.get("segment").isJsonNull() && !included.contains(text(symbol, "segment"))) return false;
        if (!symbol.get("rom").isJsonNull()) {
            long rom = number(symbol, "rom");
            for (String name : included) {
                JsonObject seg = segment(name);
                if (number(seg, "start") <= rom && rom < number(seg, "end")) return true;
            }
            return false;
        }
        return true;
    }

    private static void symbols(Program program, Set<String> included) throws Exception {
        Set<Address> primary = new HashSet<>();
        for (JsonElement element : manifest.getAsJsonArray("symbols")) {
            JsonObject symbol = element.getAsJsonObject();
            Address location = address(program, number(symbol, "address"));
            if (!symbolApplies(symbol, included) || !program.getMemory().contains(location)) continue;
            String name = text(symbol, "name");
            Symbol label = program.getSymbolTable().getSymbol(name, location, null);
            if (label == null) label = program.getSymbolTable().createLabel(location, name, SourceType.IMPORTED);
            if (primary.add(location)) label.setPrimary();
        }
    }

    private static void sourceTypes(Program program, Set<String> units) throws Exception {
        DataTypeManager manager = program.getDataTypeManager();
        for (JsonElement element : manifest.getAsJsonArray("units")) {
            JsonObject unit = element.getAsJsonObject();
            if (!units.contains(text(unit, "segment"))) continue;
            CParser parser = new CParser(manager, false, null);
            Map<String, Object> report = new LinkedHashMap<>();
            report.put("program", program.getName());
            report.put("source", text(unit, "source"));
            int tx = program.startTransaction("Import source declarations");
            try {
                StringPropertyMap provenance = program.getUsrPropertyManager().getStringPropertyMap("mg64_signature_source");
                if (provenance == null) provenance = program.getUsrPropertyManager().createStringPropertyMap("mg64_signature_source");
                Set<String> definitions = new HashSet<>();
                for (JsonElement name : unit.getAsJsonArray("definitions")) definitions.add(name.getAsString());
                parser.parse(Files.readString(Path.of(text(unit, "path"))));
                for (Map<String, DataType> types : List.of(parser.getTypes(), parser.getComposites(), parser.getEnums())) {
                    for (Map.Entry<String, DataType> entry : types.entrySet()) {
                        if (!entry.getKey().startsWith("__mg64_global_")) manager.resolve(entry.getValue(), DataTypeConflictHandler.DEFAULT_HANDLER);
                    }
                }
                int signatures = 0;
                for (Map.Entry<String, DataType> entry : parser.getFunctions().entrySet()) {
                    if (!(entry.getValue() instanceof FunctionDefinition signature)) continue;
                    for (Symbol symbol : program.getSymbolTable().getSymbols(entry.getKey())) {
                        Function function = program.getFunctionManager().getFunctionAt(symbol.getAddress());
                        if (function == null || !function.getName().equals(entry.getKey())) continue;
                        if (function.getSignatureSource() == SourceType.USER_DEFINED) continue;
                        boolean defining = definitions.contains(entry.getKey());
                        String previous = provenance.hasProperty(function.getEntryPoint()) ? provenance.getString(function.getEntryPoint()) : "";
                        if (!defining && previous.startsWith("definition:")) continue;
                        ApplyFunctionSignatureCmd apply = new ApplyFunctionSignatureCmd(function.getEntryPoint(), signature, SourceType.IMPORTED);
                        if (!apply.applyTo(program, MONITOR)) throw new IOException(apply.getStatusMsg());
                        provenance.add(function.getEntryPoint(), (defining ? "definition:" : "declaration:") + text(unit, "source"));
                        signatures++;
                    }
                }
                int globals = 0;
                for (JsonElement bindingElement : unit.getAsJsonArray("globals")) {
                    JsonObject binding = bindingElement.getAsJsonObject();
                    DataType declared;
                    if (binding.has("type_name")) {
                        String typeName = text(binding, "type_name");
                        declared = parser.getTypes().get(typeName);
                        if (declared == null) declared = parser.getComposites().get(typeName);
                        if (declared == null) declared = parser.getEnums().get(typeName);
                    } else {
                        DataType parsed = parser.getTypes().get(text(binding, "alias"));
                        declared = parsed instanceof TypeDef alias ? alias.getDataType() : parsed;
                    }
                    if (declared == null) throw new IOException("Missing parsed global binding: " + binding);
                    for (Symbol symbol : program.getSymbolTable().getSymbols(text(binding, "name"))) {
                        Address location = symbol.getAddress();
                        if (!program.getMemory().contains(location) || program.getFunctionManager().getFunctionContaining(location) != null) continue;
                        if (binding.get("static").getAsBoolean()) {
                            boolean owned = false;
                            for (JsonElement rangeElement : unit.getAsJsonArray("data_ranges")) {
                                JsonArray range = rangeElement.getAsJsonArray();
                                if (range.get(0).getAsLong() <= location.getOffset() && location.getOffset() < range.get(1).getAsLong()) owned = true;
                            }
                            if (!owned) continue;
                        }
                        DataType type = manager.resolve(declared, DataTypeConflictHandler.DEFAULT_HANDLER);
                        if (binding.get("incomplete_array").getAsBoolean()) {
                            long size = 0;
                            for (JsonElement symbolElement : manifest.getAsJsonArray("symbols")) {
                                JsonObject sized = symbolElement.getAsJsonObject();
                                if (text(sized, "name").equals(text(binding, "name")) && number(sized, "address") == location.getOffset()) size = Math.max(size, number(sized, "size"));
                            }
                            if (size <= 0 || type.getLength() <= 0 || size % type.getLength() != 0) continue;
                            type = new ArrayDataType(type, (int) (size / type.getLength()), type.getLength());
                        }
                        if (type.getLength() <= 0) continue;
                        Data existing = program.getListing().getDefinedDataAt(location);
                        if (existing != null) continue;
                        MemoryBlock block = program.getMemory().getBlock(location);
                        Address end = location.add(type.getLength() - 1);
                        if (!block.isExecute() && block.contains(end)) {
                            program.getListing().clearCodeUnits(location, end, false);
                            program.getListing().createData(location, type);
                            globals++;
                        }
                    }
                }
                report.put("signatures", signatures);
                report.put("globals", globals);
                report.put("ok", true);
            } catch (Exception error) {
                report.put("ok", false);
                report.put("error", error.toString());
                System.err.println("Source import diagnostic: " + text(unit, "source") + ": " + error);
            } finally {
                program.endTransaction(tx, Boolean.TRUE.equals(report.get("ok")));
                reports.add(report);
            }
        }
    }

    private static void initialize(Path directory) throws Exception {
        String projectName = text(manifest, "project_name");
        // GhidraProject.createProject deletes preexisting projects: never call it on one.
        if (Files.exists(directory.resolve(projectName + ".gpr")) || Files.exists(directory.resolve(projectName + ".rep"))) {
            throw new IOException("Refusing to overwrite existing Ghidra project");
        }
        Language language = DefaultLanguageService.getLanguageService().getLanguage(new LanguageID(text(manifest, "language")));
        CompilerSpec compiler = language.getCompilerSpecByID(new CompilerSpecID(text(manifest, "compiler")));
        GhidraProject project = GhidraProject.createProject(directory.toString(), projectName, false);
        try {
            Program main = new ProgramDB("main", language, compiler, CONSUMER);
            try {
                int tx = main.startTransaction("Map original ROM");
                try {
                    main.setImageBase(address(main, 0x80025c00L), true);
                    main.getOptions("MG64").setString("ROM SHA1", text(manifest, "rom_sha1"));
                    main.getOptions("MG64").setString("Manifest", directory.resolve("manifest.json").toString());
                    main.getOptions("MG64").setString("Manifest ID", text(manifest, "manifest_id"));
                    mapSegment(main, "entry");
                    mapSegment(main, "main");
                    hardware(main);
                    functions(main, "entry");
                    functions(main, "main");
                    symbols(main, Set.of("entry", "main"));
                } finally { main.endTransaction(tx, true); }
                sourceTypes(main, Set.of("sdk", "entry", "main"));
                analyze(main);
                project.getRootFolder().createFile("main", main, MONITOR);
                main.getDomainFile().save(MONITOR);
                System.out.println("MG64 imported main: " + main.getFunctionManager().getFunctionCount() + " functions");
            } finally { main.release(CONSUMER); }
            DomainFile mainFile = project.getRootFolder().getFile("main");
            DomainFolder copies = project.getRootFolder().createFolder("_import");
            for (JsonElement element : manifest.getAsJsonArray("programs")) {
                JsonObject record = element.getAsJsonObject();
                String name = text(record, "name");
                if (name.equals("main")) continue;
                DomainFile file = mainFile.copyTo(copies, MONITOR);
                file = file.setName(name).moveTo(project.getRootFolder());
                Program program = (Program) file.getDomainObject(CONSUMER, false, false, MONITOR);
                try {
                    int tx = program.startTransaction("Map isolated overlay");
                    try {
                        program.setName(name);
                        mapSegment(program, name);
                        functions(program, name);
                        symbols(program, Set.of("entry", "main", name));
                    } finally { program.endTransaction(tx, true); }
                    sourceTypes(program, Set.of(name));
                    analyze(program);
                    program.getDomainFile().save(MONITOR);
                    System.out.println("MG64 imported " + name + ": " + program.getFunctionManager().getFunctionCount() + " functions");
                } finally { program.release(CONSUMER); }
            }
            copies.delete();
        } finally { project.close(); }
        recordReady(directory);
    }

    private static void recordReady(Path directory) throws Exception {
        Files.writeString(directory.resolve("import-report.json"), JSON.toJson(reports) + "\n");
        long failures = reports.stream().filter(r -> Boolean.FALSE.equals(r.get("ok"))).count();
        if (failures != 0) throw new IOException("Source type import failed; inspect import-report.json before using this project");
        Map<String, Object> ready = new LinkedHashMap<>();
        ready.put("rom_sha1", text(manifest, "rom_sha1"));
        ready.put("manifest_id", text(manifest, "manifest_id"));
        ready.put("programs", manifest.getAsJsonArray("programs").size());
        ready.put("source_import_diagnostics", failures);
        ready.put("ghidra_home", System.getProperty("ghidra.home"));
        Files.writeString(directory.resolve("ready.json"), JSON.toJson(ready) + "\n");
        System.out.println("MG64 ready: " + JSON.toJson(ready));
    }

    private static void refresh(Path directory) throws Exception {
        GhidraProject project = GhidraProject.openProject(directory.toString(), text(manifest, "project_name"), false);
        try {
            for (JsonElement element : manifest.getAsJsonArray("programs")) {
                JsonObject record = element.getAsJsonObject();
                String name = text(record, "name");
                Set<String> included = new HashSet<>();
                for (JsonElement includedName : record.getAsJsonArray("segments")) included.add(includedName.getAsString());
                DomainFile file = project.getRootFolder().getFile(name);
                if (file == null) throw new IOException("Missing program: " + name);
                Program program = (Program) file.getDomainObject(CONSUMER, false, false, MONITOR);
                try {
                    int tx = program.startTransaction("Reconcile authoritative names and source types");
                    try {
                        for (String includedName : included) {
                            for (JsonElement functionElement : manifest.getAsJsonObject("functions").getAsJsonArray(includedName)) {
                                JsonObject functionRecord = functionElement.getAsJsonObject();
                                Function function = program.getFunctionManager().getFunctionAt(address(program, number(functionRecord, "address")));
                                if (function != null && function.getSymbol().getSource() != SourceType.USER_DEFINED) function.setName(text(functionRecord, "name"), SourceType.IMPORTED);
                            }
                        }
                        for (JsonElement symbolElement : manifest.getAsJsonArray("symbols")) {
                            JsonObject symbolRecord = symbolElement.getAsJsonObject();
                            if (symbolApplies(symbolRecord, included)) continue;
                            Symbol symbol = program.getSymbolTable().getSymbol(text(symbolRecord, "name"), address(program, number(symbolRecord, "address")), null);
                            if (symbol != null && symbol.getSource() == SourceType.IMPORTED && symbol.getSymbolType() == SymbolType.LABEL) symbol.delete();
                        }
                        symbols(program, included);
                        program.getOptions("MG64").setString("Manifest ID", text(manifest, "manifest_id"));
                    } finally { program.endTransaction(tx, true); }
                    included.add("sdk");
                    sourceTypes(program, included);
                    analyze(program);
                    program.getDomainFile().save(MONITOR);
                    System.out.println("MG64 refreshed " + name);
                } finally { program.release(CONSUMER); }
            }
        } finally { project.close(); }
        recordReady(directory);
    }

    public static void main(String[] args) throws Exception {
        manifest = JsonParser.parseString(Files.readString(Path.of(args[1]))).getAsJsonObject();
        if (args[0].equals("serve")) {
            // Preserve SDK and game spelling; this also disables Hungarian field rewriting.
            NamingPolicy.getInstance().setStrictNamingEnforcement(false, "Mario Golf source conventions");
            GhidraMCPHeadlessServer.main(new String[]{"--bind", "127.0.0.1", "--port", args[2],
                "--project", Path.of(text(manifest, "project_dir"), text(manifest, "project_name") + ".gpr").toString(), "--program", "/main"});
            return;
        }
        Application.initializeApplication(new GhidraApplicationLayout(), new HeadlessGhidraApplicationConfiguration());
        Path directory = Path.of(text(manifest, "project_dir"));
        if (args[0].equals("refresh")) refresh(directory);
        else initialize(directory);
    }
}

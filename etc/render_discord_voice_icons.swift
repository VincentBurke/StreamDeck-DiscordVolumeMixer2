import AppKit
import CoreGraphics
import Foundation

private struct DrawCommand {
    let pathData: String
    let fillColor: NSColor?
    let strokeColor: NSColor?
    let strokeWidth: CGFloat
    let translation: CGPoint
    let lineCap: CGLineCap
    let lineJoin: CGLineJoin
}

private struct IconDefinition {
    let stem: String
    let commands: [DrawCommand]
}

private let activeColor = NSColor(calibratedRed: 242.0 / 255.0, green: 243.0 / 255.0, blue: 245.0 / 255.0, alpha: 1)
private let mutedColor = NSColor(calibratedRed: 237.0 / 255.0, green: 66.0 / 255.0, blue: 69.0 / 255.0, alpha: 1)

private let micBodyPath = "M-4,-1.46C-4,-3.669 -2.209,-5.46 0,-5.46C2.209,-5.46 4,-3.669 4,-1.46V2.5C4,4.709 2.209,6.5 0,6.5C-2.209,6.5 -4,4.709 -4,2.5V-1.46Z"
private let micArcPath = "M-7,-3C-7,0.866 -3.866,4 0,4C3.866,4 7,0.866 7,-3"
private let micStemPath = "M-1,-2C-1,-2.276 -0.776,-2.5 -0.5,-2.5H0.5C0.776,-2.5 1,-2.276 1,-2V2C1,2.276 0.776,2.5 0.5,2.5H-0.5C-0.776,2.5 -1,2.276 -1,2V-2Z"
private let micBasePath = "M3,-1C3.552,-1 4,-0.552 4,0C4,0.552 3.552,1 3,1H-3C-3.552,1 -4,0.552 -4,0C-4,-0.552 -3.552,-1 -3,-1H3Z"
private let headsetPath = "M-8,-0.288C-8,-4.706 -4.418,-8.288 0,-8.288C4.418,-8.288 8,-4.706 8,-0.288C8,0.405 7.954,1.072 7.854,1.712H6C5.056,1.712 4.167,2.157 3.6,2.912L1.627,5.542C1.158,6.168 1.036,6.988 1.304,7.723C1.889,9.332 3.887,10.288 5.482,9.132C8.839,6.701 10,3.38 10,-0.288C10,-5.811 5.523,-10.288 0,-10.288C-5.523,-10.288 -10,-5.811 -10,-0.288C-10,3.38 -8.839,6.701 -5.482,9.132C-3.887,10.288 -1.889,9.332 -1.304,7.723C-1.036,6.988 -1.158,6.168 -1.627,5.542L-3.6,2.912C-4.167,2.157 -5.056,1.712 -6,1.712H-7.854C-7.954,1.072 -8,0.405 -8,-0.288Z"
private let slashPath = "M-10,10C-10,10 10,-10 10,-10"

private let icons = [
    IconDefinition(
        stem: "dvm_mic_active",
        commands: [
            DrawCommand(pathData: micBodyPath, fillColor: activeColor, strokeColor: nil, strokeWidth: 0, translation: CGPoint(x: 12, y: 8.5), lineCap: .butt, lineJoin: .miter),
            DrawCommand(pathData: micArcPath, fillColor: nil, strokeColor: activeColor, strokeWidth: 2, translation: CGPoint(x: 12, y: 14), lineCap: .round, lineJoin: .miter),
            DrawCommand(pathData: micStemPath, fillColor: activeColor, strokeColor: nil, strokeWidth: 0, translation: CGPoint(x: 12, y: 20), lineCap: .butt, lineJoin: .miter),
            DrawCommand(pathData: micBasePath, fillColor: activeColor, strokeColor: nil, strokeWidth: 0, translation: CGPoint(x: 12, y: 22), lineCap: .butt, lineJoin: .miter),
        ]
    ),
    IconDefinition(
        stem: "dvm_mic_muted",
        commands: [
            DrawCommand(pathData: micBodyPath, fillColor: mutedColor, strokeColor: nil, strokeWidth: 0, translation: CGPoint(x: 12, y: 8.5), lineCap: .butt, lineJoin: .miter),
            DrawCommand(pathData: micArcPath, fillColor: nil, strokeColor: mutedColor, strokeWidth: 2, translation: CGPoint(x: 12, y: 14), lineCap: .round, lineJoin: .miter),
            DrawCommand(pathData: micStemPath, fillColor: mutedColor, strokeColor: nil, strokeWidth: 0, translation: CGPoint(x: 12, y: 20), lineCap: .butt, lineJoin: .miter),
            DrawCommand(pathData: micBasePath, fillColor: mutedColor, strokeColor: nil, strokeWidth: 0, translation: CGPoint(x: 12, y: 22), lineCap: .butt, lineJoin: .miter),
            DrawCommand(pathData: slashPath, fillColor: nil, strokeColor: mutedColor, strokeWidth: 2, translation: CGPoint(x: 12, y: 12), lineCap: .round, lineJoin: .miter),
        ]
    ),
    IconDefinition(
        stem: "dvm_headset_active",
        commands: [
            DrawCommand(pathData: headsetPath, fillColor: activeColor, strokeColor: nil, strokeWidth: 0, translation: CGPoint(x: 12, y: 12.288), lineCap: .butt, lineJoin: .miter),
        ]
    ),
    IconDefinition(
        stem: "dvm_headset_deafened",
        commands: [
            DrawCommand(pathData: headsetPath, fillColor: mutedColor, strokeColor: nil, strokeWidth: 0, translation: CGPoint(x: 12, y: 12.288), lineCap: .butt, lineJoin: .miter),
            DrawCommand(pathData: slashPath, fillColor: nil, strokeColor: mutedColor, strokeWidth: 2, translation: CGPoint(x: 12, y: 12), lineCap: .round, lineJoin: .miter),
        ]
    ),
]

private func tokenize(_ pathData: String) -> [String] {
    let pattern = "[A-Za-z]|[-+]?(?:\\d*\\.\\d+|\\d+)"
    let regex = try! NSRegularExpression(pattern: pattern)
    let range = NSRange(pathData.startIndex..., in: pathData)
    return regex.matches(in: pathData, range: range).compactMap {
        Range($0.range, in: pathData).map { String(pathData[$0]) }
    }
}

private func isCommand(_ token: String) -> Bool {
    token.count == 1 && token.first!.isLetter
}

private func buildPath(from pathData: String, translatedBy translation: CGPoint) -> CGPath {
    let tokens = tokenize(pathData)
    let path = CGMutablePath()
    var index = 0
    var command = ""
    var currentPoint = CGPoint.zero
    var subpathStart = CGPoint.zero

    func nextNumber() -> CGFloat {
        let value = CGFloat(Double(tokens[index])!)
        index += 1
        return value
    }

    while index < tokens.count {
        if isCommand(tokens[index]) {
            command = tokens[index]
            index += 1
        }

        switch command {
        case "M":
            let point = CGPoint(x: nextNumber() + translation.x, y: nextNumber() + translation.y)
            path.move(to: point)
            currentPoint = point
            subpathStart = point
            command = "L"
        case "L":
            while index < tokens.count && !isCommand(tokens[index]) {
                let point = CGPoint(x: nextNumber() + translation.x, y: nextNumber() + translation.y)
                path.addLine(to: point)
                currentPoint = point
            }
        case "H":
            while index < tokens.count && !isCommand(tokens[index]) {
                let point = CGPoint(x: nextNumber() + translation.x, y: currentPoint.y)
                path.addLine(to: point)
                currentPoint = point
            }
        case "V":
            while index < tokens.count && !isCommand(tokens[index]) {
                let point = CGPoint(x: currentPoint.x, y: nextNumber() + translation.y)
                path.addLine(to: point)
                currentPoint = point
            }
        case "C":
            while index < tokens.count && !isCommand(tokens[index]) {
                let cp1 = CGPoint(x: nextNumber() + translation.x, y: nextNumber() + translation.y)
                let cp2 = CGPoint(x: nextNumber() + translation.x, y: nextNumber() + translation.y)
                let end = CGPoint(x: nextNumber() + translation.x, y: nextNumber() + translation.y)
                path.addCurve(to: end, control1: cp1, control2: cp2)
                currentPoint = end
            }
        case "Z":
            path.closeSubpath()
            currentPoint = subpathStart
        default:
            fatalError("Unsupported SVG path command: \(command)")
        }
    }

    return path
}

private func renderIcon(_ icon: IconDefinition, size: Int, outputURL: URL) throws {
    let rep = NSBitmapImageRep(
        bitmapDataPlanes: nil,
        pixelsWide: size,
        pixelsHigh: size,
        bitsPerSample: 8,
        samplesPerPixel: 4,
        hasAlpha: true,
        isPlanar: false,
        colorSpaceName: .deviceRGB,
        bytesPerRow: 0,
        bitsPerPixel: 0
    )!
    rep.size = NSSize(width: size, height: size)

    NSGraphicsContext.saveGraphicsState()
    let graphicsContext = NSGraphicsContext(bitmapImageRep: rep)!
    NSGraphicsContext.current = graphicsContext
    let context = graphicsContext.cgContext

    context.clear(CGRect(x: 0, y: 0, width: size, height: size))
    context.translateBy(x: 0, y: CGFloat(size))
    context.scaleBy(x: CGFloat(size) / 24.0, y: -CGFloat(size) / 24.0)
    context.setAllowsAntialiasing(true)

    for command in icon.commands {
        let path = buildPath(from: command.pathData, translatedBy: command.translation)
        if let fillColor = command.fillColor?.cgColor {
            context.addPath(path)
            context.setFillColor(fillColor)
            context.fillPath()
        }
        if let strokeColor = command.strokeColor?.cgColor {
            context.addPath(path)
            context.setStrokeColor(strokeColor)
            context.setLineWidth(command.strokeWidth)
            context.setLineCap(command.lineCap)
            context.setLineJoin(command.lineJoin)
            context.strokePath()
        }
    }

    NSGraphicsContext.restoreGraphicsState()

    guard let png = rep.representation(using: .png, properties: [:]) else {
        fatalError("Failed to create PNG representation")
    }
    try png.write(to: outputURL)
}

let outputRoot: URL
if CommandLine.arguments.count > 1 {
    outputRoot = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
} else {
    outputRoot = URL(fileURLWithPath: FileManager.default.currentDirectoryPath, isDirectory: true)
        .appendingPathComponent("dist/icons", isDirectory: true)
}

for icon in icons {
    try renderIcon(icon, size: 72, outputURL: outputRoot.appendingPathComponent("\(icon.stem)_72px.png"))
    try renderIcon(icon, size: 144, outputURL: outputRoot.appendingPathComponent("\(icon.stem)_72px@2x.png"))
}
